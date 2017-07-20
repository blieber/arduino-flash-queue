#ifndef FLASHQUEUE
#define FLASHQUEUE

#include <deque>
#include <algorithm> // std::sort
#include <ArduinoLog.h> // Convenient logging library that wraps Arduino Serial


template <typename HeaderMetadataType> struct FlashPageHeader
{
    // Pointer to own flash page number
    // Also used to check if page has been initialized previously
    int pageNumber;

    // Number of stored data packets
    int count;

    // Incremented with every flash write - most recent has highest number.
    // This is sufficiently large should not have to worry about overflow as
    // flash is rated for 10,000 writes << 2^31
    int flashWriteId;

    // (Optional) custom metadata associated with each flash page
    HeaderMetadataType metadata;

    FlashPageHeader(int pageNumber, int count, int flashWriteId, const HeaderMetadataType& metadata) :
        pageNumber(pageNumber),
        count(count),
        flashWriteId(flashWriteId),
        metadata(metadata)
    {
    }
};


// A naive implementation which allows client to:
// 1. Write fixed number of data packets to new flash pages
// 2. Peek and pop data packets from existing flash pages:
//   a. Packets are dequeued in the order they are enqueued (FIFO)
//   b. Only opportunistically writes back page erase to flash memory,
//      meaning if device is shut down / reset not all deletes may go
//      through resulting in redundant data
template <typename DataType, typename HeaderMetadataType = int> class FlashQueue
{
public:
    const int DataPacketsPerFlashPage;

    // Total available number flash pages
    const int NumFlashPages;

    FlashQueue(int bytesPerFlashPage, int lowestFlashPageAvailable, int highestFlashPageAvailable) :
      DataPacketsPerFlashPage((bytesPerFlashPage - sizeof(FlashPageHeader<HeaderMetadataType>)) / sizeof(DataType)),
      LowestFlashPageNumber(lowestFlashPageAvailable),
      NumFlashPages(highestFlashPageAvailable - lowestFlashPageAvailable + 1),
      readDataPeekIndex(0)
    {
        Log.trace("Initializing flash translation layer with DataPacketsPerFlashPage=%d, LowestFlashPageNumber=%d, NumFlashPages=%d\n", DataPacketsPerFlashPage, LowestFlashPageNumber, NumFlashPages);
        Log.verbose("FlashPageHeader size = %d, DataType size = %d\n", sizeof(FlashPageHeader<HeaderMetadataType>), sizeof(DataType));

        // Loop through flash pages and initialize internal understanding of flash availability.
        for (int page = LowestFlashPageNumber; page < LowestFlashPageNumber + NumFlashPages; page++)
        {
            auto header = reinterpret_cast<FlashPageHeader<HeaderMetadataType>*>(ADDRESS_OF_PAGE(page));
            if (page == header->pageNumber)
            {
                dataFlashPages.push_back(*header);
            }
            else
            {
                emptyFlashPages.push_back(page);
            }
        }

        // Sort queue by id - the earliest data (lowest id) should be at the start of the queue
        std::sort(
            dataFlashPages.begin(), 
            dataFlashPages.end(), 
            // Style note - C++14 apparently allows use of "auto" here http://stackoverflow.com/questions/1380463/sorting-a-vector-of-custom-objects
            [](FlashPageHeader<HeaderMetadataType> const& lhs, FlashPageHeader<HeaderMetadataType> const& rhs)
            {
               return lhs.flashWriteId < rhs.flashWriteId;
            });

        Log.trace("Found %d data flash pages and %d empty flash pages\n", dataFlashPages.size(), emptyFlashPages.size());
    }

    // Write data packets to new flash page
    // Note: expects and writes DataPacketsPerFlashPage
    bool flashWriteData(const DataType* data, const HeaderMetadataType& metadata = 0)
    {
        if (emptyFlashPages.empty())
        {
            Log.error("Cannot save new data packet - no more space!\n");
            return false;
        }

        int page = emptyFlashPages.front();

        // Increment last flash write id by 1 to get this page's id
        int flashWriteId = dataFlashPages.empty() 
            ? 0 
            : dataFlashPages.back().flashWriteId + 1;

        if (!writeDataPacketsToFlashPage(page, flashWriteId, metadata, data, DataPacketsPerFlashPage))
        {
            Log.error("Failed writing data packets to flash.");
            return false;
        }

        // sanity check that write actually went through
        auto header = reinterpret_cast<FlashPageHeader<HeaderMetadataType>*>(ADDRESS_OF_PAGE(page));
        if (page != header->pageNumber) 
        {
            Log.error("Unknown error writing data packets to flash.");
            return false;
        }

        emptyFlashPages.pop_front();
        dataFlashPages.push_back(*header);
        return true;
    }

    bool flashStorageFull() const
    {
        return emptyFlashPages.empty();
    }

    bool flashDataAvailable() const
    {
        return !dataFlashPages.empty();
    }

    // Peek at next available data packet
    // Undefined behavior if no data avaialble
    DataType flashPeekDatum() const
    {
        int page = dataFlashPages.front().pageNumber;
        DataType* data = getDataOnPage(page);
        return data[readDataPeekIndex];
    }

    // "Pop" data packet and seek to next data packet
    bool flashPopDatum()
    {
        if (dataFlashPages.empty())
        {
            Log.error("No flash data to pop.\n");
            return false;
        }

        readDataPeekIndex++;
        int numDataPacketsOnPage = dataFlashPages.front().count;
        if (readDataPeekIndex < numDataPacketsOnPage)
        {
            Log.verbose("Simulating erase by moving peek pointer to next data packet %d\n", readDataPeekIndex);
            return true;
        }
      
        // Erase flash page and move to next one
        else
        {
            int page = dataFlashPages.front().pageNumber;
            Log.verbose("Erasing all data packets contained in flash page %d", page);

            if (clearFlashPage(page))
            {
              readDataPeekIndex = 0;
              dataFlashPages.pop_front();
              emptyFlashPages.push_back(page);
              return true;
            }
            // error erasing flash page - cannot proceed!
            else
            {
              Log.error("Error erasing flash page.\n");
              readDataPeekIndex--; // restore old index
              return false;
            }
        }
    }

private:
    // First available flash page number
    const int LowestFlashPageNumber;

    // Which data packet currently peeking from within page.
    int readDataPeekIndex;

    // Queue containing cached info of all flash pages that contain data
    std::deque<FlashPageHeader<HeaderMetadataType>> dataFlashPages;

    // Queue containing cached info of numbers of all empty flash pages
    std::deque<int> emptyFlashPages;

    static bool writeDataPacketsToFlashPage(int page, int flashWriteId, const HeaderMetadataType& metadata, const DataType* data, int numDataPackets)
    {
        // Ignore possible overflow error numDataPackets for now
        FlashPageHeader<HeaderMetadataType> header(page, numDataPackets, flashWriteId, metadata);

        int rc = eraseThenWriteFlashPage(page, &header, sizeof(FlashPageHeader<HeaderMetadataType>), data, numDataPackets * sizeof(DataType));

        return rc == 0;
    } 

    static bool clearFlashPage(int page)
    {
        // TODO - more efficient way to do this than creating an empty struct on stack
        char zeroBuffer[sizeof(FlashPageHeader<HeaderMetadataType>)] = {};
        
        int rc = eraseThenWriteFlashPage(
            page, 
            &zeroBuffer, 
            sizeof(zeroBuffer), 
            nullptr, 
            0);
        return rc == 0;
    }

    static DataType* getDataOnPage(int page)
    {
        return reinterpret_cast<DataType*>(((void *)ADDRESS_OF_PAGE(page)) + sizeof(FlashPageHeader<HeaderMetadataType>));
    }

    // Needs to be followed with a flash page write - otherwise controller restarts
    // Should still be 4 bytes aligned due to underlying impl of flashWriteBlock
    static int eraseFlashPage(int page)
    {
        Log.verbose("Attempting to erase flash page %d\n", page);

        int rc = flashPageErase(page);
        if (rc == 0)
        {
            Log.verbose("Flash page erase success\n");
        }
        else if (rc == 1)
        {
            Log.error("Error - the flash page is reserved\n");
        }
        else if (rc == 2)
        {
            Log.error("Error - the flash page is used by the sketch\n");
        }

        return rc;
    }

    // Should still be 4 bytes aligned due to underlying impl of flashWriteBlock
    static int eraseThenWriteFlashPage(int page, const void* header, int numBytesHeader, const void* data, int numBytesData)
    {
        // Argument check
        if (PAGE_FROM_ADDRESS(((void *)ADDRESS_OF_PAGE(page)) + numBytesHeader + numBytesData - 1) != page)
        {
            Log.error("Invalid arguments - more data to write to page than can fit on page!\n");
            Log.trace("Num bytes header = %d, num bytes data = %d\n", numBytesHeader, numBytesData);
            return -1;
        }

        int rc = eraseFlashPage(page);
        if (rc != 0)
        {
            return rc;
        }

        Log.verbose("Attempting to write header to flash page %d (%x)\n", page, ((void *)ADDRESS_OF_PAGE(page)));
        rc = flashWriteBlock(((void *)ADDRESS_OF_PAGE(page)), header, numBytesHeader);
        if (rc == 0)
        {
            Log.verbose("Flash page header write success\n");
        }
        else if (rc == 1)
        {
            Log.error("Error - the flash page is reserved\n");
            return rc;
        }
        else if (rc == 2)
        {
            Log.error("Error - the flash page is used by the sketch\n");
            return rc;
        }

        Log.verbose("Attempting to write data to flash page %d (%x)\n", page, ((void *)ADDRESS_OF_PAGE(page)) + numBytesHeader);
        rc = flashWriteBlock(((void *)ADDRESS_OF_PAGE(page)) + numBytesHeader, data, numBytesData); 
        if (rc == 0)
        {
            Log.verbose("Flash page data write success\n");
        }
        else if (rc == 1)
        {
            Log.error("Error - the flash page is reserved\n");
        }
        else if (rc == 2)
        {
            Log.error("Error - the flash page is used by the sketch\n");
        }

        return rc;
    }
};

#endif // FLASHQUEUE
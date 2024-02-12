//
// CMP202
// Javad Zarrin, j.zarrin@abertay.ac.uk
// Thread Overhead - Atomics
//------------------------------------------------------------
// Include necessary headers for platform-specific code
#if defined(_WIN32)
    #include <windows.h>  // Windows-specific header for system calls
    #include <psapi.h>
#elif defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>  // UNIX-specific header, used in Linux and macOS
    #if defined(__APPLE__)
        #include <mach/mach.h>  // Additional macOS-specific header for system calls
    #else
        #include <sys/resource.h>  // Linux-specific header for resource usage
    #endif
#endif

//------------------------------------------------------------
#include <iostream>    // For input and output
#include <vector>      // For using the vector container
#include <thread>      // For creating threads
#include <random>      // For generating random numbers
//#include <unistd.h>
//#include <sys/resource.h>
#include <atomic>
#include <mutex>
#include <climits> // Include for INT_MAX

const int THRESHOLD=10000000;//10'000'000
const size_t number_of_elements = 100; //Try 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000, and 1'000'000'000 depending on your computer resources. 
//Stop execution if it exeeds more than 4-5 minutes and try differn configs.


std::mutex cout_mutex;

std::atomic<int> numThreads(0); 

// Function to merge two halves of a vector
void merge(std::vector<int>& array, int left, int mid, int right) {
    // Calculate the sizes of the two subarrays to be merged
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Create temporary vectors for the two halves
    std::vector<int> L(n1), R(n2);

    // Copy data to the temporary vectors
    for (int i = 0; i < n1; i++)
        L[i] = array[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = array[mid + 1 + j];

    // Initial indexes for the sub-vectors and the merged vector
    int i = 0, j = 0, k = left;

    // Merge the temporary arrays back into the original array
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k] = L[i];
            i++;
        } else {
            array[k] = R[j];
            j++;
        }
        k++;
    }

    // Copy the remaining elements of L[], if any
    while (i < n1) {
        array[k] = L[i];
        i++;
        k++;
    }

    // Copy the remaining elements of R[], if any
    while (j < n2) {
        array[k] = R[j];
        j++;
        k++;
    }
}

// Function to implement mergesort
void mergesort(std::vector<int>& array, int left, int right) {
    if (left < right) {
        // Find the middle point to divide the array into halves
        int mid = left + (right - left) / 2;

        // Call mergesort for the first half
        mergesort(array, left, mid);
        // Call mergesort for the second half
        mergesort(array, mid + 1, right);

        // Merge the two halves sorted in the previous steps
        merge(array, left, mid, right);
    }
}

void parallel_mergesort(std::vector<int>& array, int left, int right) {
    
    cout_mutex.lock();
    std::cout << "--Thread" << numThreads << ": start=" << left << " end=" << right << std::endl;
    cout_mutex.unlock();

    if (left < right) {
        // Same as above, find the middle of the array
        int mid = left + (right - left) / 2;
        

        // Create two threads for sorting each half of the array
        numThreads += 2;

        std::thread leftThread(parallel_mergesort, std::ref(array), left, mid);
        std::thread rightThread(parallel_mergesort, std::ref(array), mid + 1, right);

        // Wait for both threads to finish
        leftThread.join();
        rightThread.join();
      

        // Merge the sorted halves
        merge(array, left, mid, right);
    }
}

// Partially parallel version of the mergesort function (combination of threading and recursion)
void partial_parallel_mergesort(std::vector<int>& array, int left, int right, int depth) {
    if (left < right) {
        // Declare 'mid' outside the if condition so it's accessible throughout the function
        int mid = left + (right - left) / 2;

        int size = right - left + 1;
        if (size > THRESHOLD && depth > 0) {
            // Increment thread count for new threads being created
            numThreads.fetch_add(2, std::memory_order_relaxed);

            // Parallel sorting for larger segments
            std::thread leftThread([=, &array]() { partial_parallel_mergesort(array, left, mid, depth - 1); });
            std::thread rightThread([=, &array]() { partial_parallel_mergesort(array, mid + 1, right, depth - 1); });

            leftThread.join();
            rightThread.join();
        }
        else {
            // Sequential sorting for smaller segments or when maximum depth is reached
            mergesort(array, left, mid);
            mergesort(array, mid + 1, right);
        }

        // Now 'mid' is defined in this scope, allowing the merge to proceed without error
        merge(array, left, mid, right);
    }
}



// Function to print only the first 20 elements of the vector
void printVector(const std::vector<int>& v) {
    int elements = 0;
    for (int num : v) {
        if (elements<20) std::cout << num << " ";
        elements++;
    }
    std::cout << std::endl;
}


int main() {
    // Create a vector to store N random numbers
    
    std::vector<int> array(number_of_elements);

    // Initialize a random number generator
    std::random_device rd;               // Non-deterministic generator
    std::mt19937 gen(rd());              // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> dis(1, number_of_elements); // INT_MAX); // Uniform distribution between 1 and N

    // Fill the vector with random numbers
    for (int& number : array) {
        number = dis(gen);
    }

    std::cout << "Size of the vector to be sorted : "<< number_of_elements << std::endl;

    // Print the original, unsorted vector
    std::cout << "Original Vector:" << std::endl;
    printVector(array);
	// Measure the time taken by sequential mergesort
	auto start = std::chrono::high_resolution_clock::now();
	mergesort(array, 0, array.size() - 1);  // Perform standard mergesort on the entire array
	auto end = std::chrono::high_resolution_clock::now();
	// Calculate and print the duration for sequential sort
	auto durationSeq = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	std::cout << "Sorted Vector (Sequential):" << std::endl;
	printVector(array);  // Print the sorted vector
	std::cout << "Execution Time (Sequential): " << durationSeq << " nanoseconds" << std::endl;

	// Shuffle array again to unsort it for parallel sort
	std::shuffle(array.begin(), array.end(), gen);  // Shuffle using the same random number generator

	// Measure the time taken by parallel mergesort
	int maxDepth = 2; // Define the depth of parallelism
	start = std::chrono::high_resolution_clock::now();

	//parallel_mergesort(array, 0, array.size() - 1);  // Perform parallel mergesort on the entire array
    partial_parallel_mergesort(array, 0, array.size() - 1, maxDepth);  // Perform partial parallel mergesort on the entire array

	end = std::chrono::high_resolution_clock::now();
	// Calculate and print the duration for parallel sort
	auto durationPar = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(); // change to nanoseconds or milliseconds if needed
    // for example if it measure the execution time =0 seconds then use smaller time units like milli or nano 
	std::cout << "Sorted Vector (Parallel):" << std::endl;
	printVector(array);  // Print the sorted vector
	std::cout << "Execution Time (Parallel): " << durationPar << " nanoseconds" << std::endl;

	// Calculate and print the speedup achieved by parallelisation
	double speedup = static_cast<double>(durationSeq) / durationPar;
	std::cout << "Speedup: " << speedup << "x" << std::endl;

 //-----------------------------------------------------------------------------------------------------
 // Print the number of CPU cores
	unsigned int numCores = std::thread::hardware_concurrency();
	// This function is part of the standard library and works across all platforms.
	std::cout << "Number of CPU cores: " << numCores << std::endl;

// Platform-specific memory usage information for the current program or process
	#if defined(_WIN32)
		// Windows specific memory usage
		PROCESS_MEMORY_COUNTERS pmc;  // Struct to store memory information
		GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
		// Retrieves memory information about the current process
		SIZE_T physMemUsedByMe = pmc.WorkingSetSize;  // Extracts the working set size (current memory usage)
		std::cout << "Memory Usage for the Process (Windows): " << physMemUsedByMe/(1024*1024) << " Mbytes" << std::endl; // Change the memory unit if needed like KB, MB
	#elif defined(__unix__)
		// UNIX/Linux specific memory usage
		struct rusage usage;  // Struct to store resource usage statistics
		getrusage(RUSAGE_SELF, &usage);
		// Fills the 'usage' struct with information about the current process (RUSAGE_SELF)
		std::cout << "Memory Usage for the Process (Unix/Linux): " << usage.ru_maxrss << " kilobytes" << std::endl;
		// Prints the maximum resident set size (peak memory usage)
	#elif defined(__APPLE__)
		// macOS specific memory usage
		struct mach_task_basic_info info;  // Struct to store memory information for macOS
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
		// Represents the count of information elements in 'info'
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
			// Retrieves memory information about the current process
			std::cout << "Memory Usage for the Process (macOS): " << info.resident_size << " bytes" << std::endl;
			// Prints the resident size (current memory usage) in bytes
		}
	#endif

	//-----------------------------------------------------------------------------------------------------
        std::cout << "A total of " << numThreads.load()<< " threads are created (in addition to the main thread)." << std::endl;

	return 0;
}



/* Sample output with 6x+ speedup --- 
Size of the vector to be sorted : 100000000
Original Vector:
26182689 68039662 9915935 85806420 79331798 68639586 65920871 32041564 19946464 41745542 37363510 94901895 59195224 49062038 96342485 4328162 69948181 94401606 64592255 60638091
Sorted Vector (Sequential):
1 3 4 4 5 5 5 6 6 7 9 11 12 13 13 13 14 14 15 16
Execution Time (Sequential): 30911851400 nanoseconds
Sorted Vector (Parallel):
1 3 4 4 5 5 5 6 6 7 9 11 12 13 13 13 14 14 15 16
Execution Time (Parallel): 4768680600 nanoseconds
Speedup: 6.48227x
Number of CPU cores: 32
Memory Usage for the Process (Windows): 388 Mbytes
A total of 61 threads are created (in addition to the main thread).



----------------------------------------------------
* Sample output with 7x+ speedup ---
Size of the vector to be sorted : 1000000000
Original Vector:
219401300 415206301 911559229 610146411 888814505 352686763 738483721 821035590 694685487 441384665 76070690 166542903 796659440 776214367 712733308 513649943 378639218 588343824 947929069 416472838
Sorted Vector (Sequential):
2 3 4 4 5 5 5 7 7 9 9 10 10 11 14 15 15 19 19 19
Execution Time (Sequential): 372892926000 nanoseconds
Sorted Vector (Parallel):
2 3 4 4 5 5 5 7 7 9 9 10 10 11 14 15 15 19 19 19
Execution Time (Parallel): 48564068000 nanoseconds
Speedup: 7.67837x
Number of CPU cores: 32
Memory Usage for the Process (Windows): 3831 Mbytes
A total of 509 threads are created (in addition to the main thread).






*/
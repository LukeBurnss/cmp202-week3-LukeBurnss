// CMP202
// Elevator System Simulation
// Javad Zarrin, j.zarrin@abertay.ac.uk
// Semaphores, Atomics, Mutex, Deadlock and more 


#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <map>
#include <random> 
using namespace std;

// Constants to define the elevator system's properties
constexpr int TOTAL_LEVELS = 10; // Total number of floors in the building
constexpr int MAX_CAPACITY = 3; // Maximum number of passengers the elevator can hold
constexpr int MOVE_DURATION = 2000; // Time in milliseconds it takes the elevator to move between floors
constexpr int STOP_DURATION = 500; // Time in milliseconds for the elevator to stop at a floor (not used here)

// Global synchronization primitives and variables
// Semaphore to limit elevator capacity -- Todo Task 2
std::atomic<int> currentLevel(0); // Current floor of the elevator, starting at ground floor (0)
std::atomic<bool> movingUp(true); // Direction of elevator movement, initially set to move up 

std::atomic<int> passengersInElevator(0); // Number of passengers currently in the elevator
std::mutex coutMutex; // Mutex for synchronizing access to console output
std::mutex elevatorMutex; // Mutex for synchronizing access to elevator-specific operations

// Data structures for tracking passengers
vector<pair<int, int>> passengersInsideElevator; // Vector to keep track of passengers inside the elevator (ID, destination floor)
std::mutex passengersInsideElevatorMutex; // Mutex for synchronizing access to the passengersInsideElevator vector
map<int, vector<pair<int, int>>> pickupRequests; // Map to hold pickup requests per floor (floor number, vector of (ID, destination floor))
std::mutex pickupRequestsMutex; // Mutex for synchronizing access to pickup requests
map<int, vector<pair<int, int>>> deliveredPassengers; // Map to hold delivered passengers per floor
std::mutex deliveredPassengersMutex; // Mutex for synchronizing access to deliveredPassengers map

std::counting_semaphore<MAX_CAPACITY> elevatorCapacity(MAX_CAPACITY);

// Function to clear the console. The implementation depends on the operating system
void clearConsole() {
#ifdef _WIN32
    system("cls"); // Clears console on Windows systems
#else
    system("clear"); // Clears console on Unix/Linux systems
#endif
}
// Function to print the current state of the building and elevator.
void printBuilding() {
    lock_guard<mutex> lock(coutMutex); // Ensures exclusive access to std::cout.
    clearConsole(); // Clears the console for a fresh display of the building state.
    for (int i = TOTAL_LEVELS - 1; i >= 0; --i) { // Iterates over each floor from top to bottom.
        cout << "Floor " << i << ": ";
        if (i == currentLevel) { // Checks if the elevator is at the current floor.
            // Displays the elevator's status, direction, and passenger count.
            cout << "[E: ";
            cout << (movingUp ? "Up" : "Down") << ", P: " << passengersInElevator.load() << "/" << MAX_CAPACITY << " ";
            for (size_t index = 0; index < passengersInsideElevator.size(); ++index) { // Iterates over passengers in the elevator.
                const auto& passenger = passengersInsideElevator[index];
                // Displays passenger ID and destination floor with color coding.
                cout << "\033[35m" << passenger.first << "t" << passenger.second << "\033[0m";
                if (index < passengersInsideElevator.size() - 1) {
                    cout << ", "; // Comma between passengers for readability.
                }
            }
            cout << "] ";
        }
        else {
            // Fills the line for floors without the elevator for alignment.
            cout << "                                     ";
        }

        // Delivered passengers
        auto& delivered = deliveredPassengers[i];
        for (auto& passenger : delivered) { // Iterates over delivered passengers at the current floor.
            // Displays delivered passenger ID and destination floor with color coding.
            cout << "\033[32m" << passenger.first << "t" << passenger.second << "\033[0m ";
        }

        // Waiting passengers
        auto& waiting = pickupRequests[i];
        for (auto& passenger : waiting) { // Iterates over waiting passengers at the current floor.
            // Displays waiting passenger ID and destination floor with color coding.
            cout << "  \033[31m" << passenger.first << "t" << passenger.second << "\033[0m ";
        }

        cout << endl; // New line for the next floor.
    }
    cout << "\n---------------------------------------\n"; 
}

// Passenger class definition
class Passenger {
public:
    int id; // Unique identifier for the passenger.
    int startFloor; // Floor where the passenger will board the elevator.
    int destinationFloor; // Passenger's destination floor.

    // Constructor to initialize a passenger with an ID, start floor, and destination floor.
    Passenger(int id, int start, int destination) : id(id), startFloor(start), destinationFloor(destination) {}

    // Function operator to simulate passenger behavior.
    void operator()() {
        {
            lock_guard<mutex> lockP(pickupRequestsMutex); // Protects addition of pickup request.
            pickupRequests[startFloor].push_back(pair(id, destinationFloor)); // Adds pickup request for this passenger.
        }
        // Wait for the elevator to arrive at the start floor.
        while (currentLevel != startFloor) {
            this_thread::sleep_for(chrono::milliseconds(100)); // Sleeps to reduce CPU usage while waiting.
        }
        // Waits until there's space in the elevator, simulating boarding. Todo Task2
        elevatorCapacity.acquire();
        {
            // Locks for dealing with pickup and inside-elevator passenger lists without causing deadlocks.
            std::unique_lock<std::mutex> lk1(pickupRequestsMutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(passengersInsideElevatorMutex, std::defer_lock);
            std::lock(lk1, lk2); // Locks both mutexes at once to prevent deadlock.

            // Finds this passenger in the pickupRequests to remove them, indicating they're now inside the elevator.
            auto itp = std::find_if(pickupRequests[startFloor].begin(), pickupRequests[startFloor].end(), [&](const auto& p) { return p.first == id; });
            if (itp != pickupRequests[startFloor].end()) {
                pickupRequests[startFloor].erase(itp);
                passengersInsideElevator.emplace_back(id, destinationFloor);
            }
        } // automatically release mutexes
        

        passengersInElevator++; // Increments the passenger count in the elevator.

        printBuilding(); // Updates the building display to reflect the new state.
        
        // Waits for the elevator to reach the destination floor.
        while (currentLevel != destinationFloor) {
            this_thread::sleep_for(chrono::milliseconds(100)); // Sleeps to reduce CPU usage while waiting.
        }

        // Exiting the elevator
        {
            std::unique_lock<std::mutex> lk1(passengersInsideElevatorMutex);
            std::unique_lock<std::mutex> lk2(deliveredPassengersMutex);
            auto it = std::find_if(passengersInsideElevator.begin(), passengersInsideElevator.end(), [&](const auto& p) { return p.first == id; });
            if (it != passengersInsideElevator.end()) {
                passengersInsideElevator.erase(it);
                deliveredPassengers[destinationFloor].emplace_back(id, destinationFloor);
            }
        } // Mutexes automatically unlock here

        passengersInElevator--; // Safe to decrement after safely exiting
        elevatorCapacity.release(); // Signal space availability
        printBuilding(); // Reflect final state changes
    
        {
            lock_guard<mutex> lock(elevatorMutex); // Protects elevator state changes.
            printBuilding(); // Updates the building display.
            cout << "Passenger " << id << " exited at floor " << destinationFloor << endl;
        }
    }
};

// The elevator function simulates the movement of the elevator through the building.
void elevator() {
    while (true) { // Infinite loop to keep the elevator moving.
        this_thread::sleep_for(chrono::milliseconds(MOVE_DURATION)); // Waits for MOVE_DURATION, simulating floor-to-floor movement.
        currentLevel += movingUp ? 1 : -1; // Moves the elevator up or down based on the direction.
        if (currentLevel == 0 || currentLevel == TOTAL_LEVELS - 1) movingUp = !movingUp; // Reverses direction at the top or bottom floor.

        {
            lock_guard<mutex> lock(elevatorMutex); // Protects elevator state changes.
            printBuilding(); // Updates the building display after each move.
        }
    }
}


int main() {
    vector<thread> threads; // Stores threads for each passenger.
    thread elevatorThread(elevator); // Thread for the elevator's continuous operation.

    std::random_device rd; // Non-deterministic random number generator.
    std::mt19937 rng(rd()); // Random number generator using Mersenne Twister algorithm.
    std::uniform_int_distribution<int> uni(0, TOTAL_LEVELS - 1); // Distribution for floor numbers.

    // Creates passenger threads with random start and destination floors.
    for (int i = 0; i < 10; i++) {
        int startFloor = uni(rng); // Random start floor.
        int destinationFloor;
        do {
            destinationFloor = uni(rng); // Ensures destination floor is different from start floor.
        } while (destinationFloor == startFloor);

        threads.emplace_back(Passenger(i, startFloor, destinationFloor)); // Adds a new passenger thread.
    }

    for (auto& t : threads) { // Joins all passenger threads to the main thread to ensure completion.
        if (t.joinable()) {
            t.join();
        }
    }

    elevatorThread.detach(); // Allows the elevator thread to run independently of the main thread.

    return 0; 
}
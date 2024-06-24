#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <vector>
#include <chrono>

using namespace std;

// Klasa Lightswitch do zarządzania wieloma wątkami odczytu
class Lightswitch {
public:
    Lightswitch() : counter(0) {}

    // Metoda blokująca dostęp dla writerów podczas obecności czytelników
    void lock(condition_variable& cv, mutex& m) {
        unique_lock<mutex> lock(mutex_);
        counter++;
        if (counter == 1) {
            unique_lock<mutex> roomLock(m);
            cv.wait(roomLock);  // Oczekiwanie, gdy writer jest obecny
        }
    }

    // Metoda odblokowująca dostęp dla writerów po zakończeniu czytania przez ostatniego czytelnika
    void unlock(condition_variable& cv, mutex& m) {
        unique_lock<mutex> lock(mutex_);
        counter--;
        if (counter == 0) {
            cv.notify_one();  // Powiadomienie oczekujących writerów
        }
    }

private:
    int counter; // Licznik czytelników
    mutex mutex_; // Mutex do ochrony dostępu do licznika
};

// Instancja Lightswitch do zarządzania czytelnikami
Lightswitch readSwitch;
mutex roomEmptyMutex; // Mutex sygnalizujący czy pokój jest pusty (brak writerów)
condition_variable roomEmptyCV; // Zmienna warunkowa do sygnalizacji pustego pokoju
mutex turnstileMutex; // Mutex do zarządzania dostępem do pokoju (kolejka)

struct SharedData {
    int value = -1; // Wartość współdzielona
    int readCount = 0; // Licznik czytelników
    mutex dataMutex; // Mutex do ochrony dostępu do danych
    mutex readCountMutex; // Mutex do ochrony licznika czytelników
};

// Instancja danych współdzielonych
SharedData sharedData;

// Funkcja reprezentująca zachowanie writerów
void writer() {
    while (true) {
        {
            unique_lock<mutex> turnstileLock(turnstileMutex); // Zablokowanie dostępu do pokoju
            unique_lock<mutex> roomEmptyLock(roomEmptyMutex); // Zablokowanie dostępu do pokoju, gdy writer jest obecny
            turnstileLock.unlock(); // Zwolnienie blokady dostępu do pokoju

            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(0, 100);
            {
                lock_guard<mutex> lock(sharedData.dataMutex); // Blokada dostępu do danych
                sharedData.value = dis(gen); // Zapisanie losowej wartości
                cout << this_thread::get_id() << ": Writing data " << sharedData.value << endl;
            }
            this_thread::sleep_for(chrono::seconds(rd() % 6 + 5));  // Losowe opóźnienie 5-10 sekund
        }
        roomEmptyCV.notify_all(); // Powiadomienie wszystkich czytelników

        unique_lock<mutex> readCountLock(sharedData.readCountMutex); // Blokada dostępu do licznika czytelników
        while (sharedData.readCount < 3) { // Oczekiwanie, aż liczba czytelników osiągnie 3
            readCountLock.unlock();
            this_thread::sleep_for(chrono::milliseconds(100)); // Krótkie opóźnienie
            readCountLock.lock();
        }
        sharedData.readCount = 0; // Zresetowanie licznika czytelników
    }
}

// Funkcja reprezentująca zachowanie czytelników
void reader() {
    while (true) {
        {
            unique_lock<mutex> turnstileLock(turnstileMutex); // Blokada dostępu do pokoju
            turnstileLock.unlock();  // Zwolnienie blokady dostępu do pokoju

            readSwitch.lock(roomEmptyCV, roomEmptyMutex); // Zablokowanie writerów, gdy są czytelnicy
            {
                lock_guard<mutex> lock(sharedData.dataMutex); // Blokada dostępu do danych
                cout << this_thread::get_id() << ": Reading data " << sharedData.value << endl;
            }
            this_thread::sleep_for(chrono::seconds(random_device{}() % 3 + 1));  // Losowe opóźnienie 1-7 sekund
            readSwitch.unlock(roomEmptyCV, roomEmptyMutex); // Zwolnienie blokady writerów po zakończeniu czytania
        }

        unique_lock<mutex> readCountLock(sharedData.readCountMutex); // Blokada dostępu do licznika czytelników
        sharedData.readCount++; // Zwiększenie licznika czytelników
        if (sharedData.readCount >= 3) { // Jeśli liczba czytelników osiągnie 3, powiadom writerów
            readCountLock.unlock();
            roomEmptyCV.notify_one();
        }
    }
}

int main() {
    vector<thread> threads; // Wektor wątków

    // Tworzenie wątków writerów
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back(writer);
    }

    // Tworzenie wątków czytelników
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(reader);
    }

    // Oczekiwanie na zakończenie wątków
    for (auto& thread : threads) {
        thread.join();
    }

    return 0; // Zakończenie programu
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <deque>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>


using namespace std;
using namespace std::chrono;


bool isPrime(uint64_t number) {
    if (number < 2) return false;
    if (number == 2) return true;
    if (number % 2 == 0) return false;

    for (uint64_t divisor = 3; divisor <= number / divisor; divisor += 2) {
        if (number % divisor == 0) return false;
    }
    return true;
}

uint64_t nthPrime(size_t n) {
    if (n == 0) throw invalid_argument("n має бути більшим за 0.");
    if (n == 1) return 2;
    if (n == 2) return 3;
    if (n == 3) return 5;
    if (n == 4) return 7;
    if (n == 5) return 11;

   
    const double nd = static_cast<double>(n);
    size_t limit = static_cast<size_t>(nd * (log(nd) + log(log(nd))) + 20.0);
    vector<bool> prime(limit + 1, true);
    prime[0] = prime[1] = false;

    for (size_t p = 2; p <= limit / p; ++p) {
        if (prime[p]) {
            for (size_t multiple = p * p; multiple <= limit; multiple += p)
                prime[multiple] = false;
        }
    }

    size_t count = 0;
    for (size_t value = 2; value <= limit; ++value) {
        if (prime[value] && ++count == n)
            return static_cast<uint64_t>(value);
    }

    // Запасний варіант на випадок, якщо оцінки limit виявилося недостатньо.
    size_t candidate = limit + 1;
    while (true) {
        if (isPrime(candidate) && ++count == n)
            return static_cast<uint64_t>(candidate);
        ++candidate;
    }
}

bool readPositiveN(const string& prompt, size_t& n, size_t maxN = 1000000) {
    cout << prompt;
    if (!(cin >> n)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Помилка: введіть ціле додатне число.\n";
        return false;
    }
    if (n == 0 || n > maxN) {
        cout << "Число має бути в діапазоні від 1 до " << maxN << ".\n";
        return false;
    }
    return true;
}

// Завдання 1.2.1
// DataPreparation від'єднується; DataProcessing опитує захищений прапорець
// з інтервалом 100 мс, використовуючи unique_lock.

namespace task121 {
deque<int> dataQueue;
mutex dataMutex;
bool dataReady = false;

void DataPreparation() {
    cout << "Введіть цілі числа (для завершення введіть q):\n";

    string token;
    while (cin >> token) {
        if (token == "q" || token == "Q") break;

        try {
            size_t parsed = 0;
            long long value = stoll(token, &parsed);
            if (parsed != token.size() ||
                value < numeric_limits<int>::min() ||
                value > numeric_limits<int>::max()) {
                cout << "Пропущено некоректне ціле число: " << token << '\n';
                continue;
            }
            lock_guard<mutex> lock(dataMutex);
            dataQueue.push_back(static_cast<int>(value));
        } catch (...) {
            cout << "Пропущено некоректне ціле число: " << token << '\n';
        }
    }

    {
        lock_guard<mutex> lock(dataMutex);
        dataReady = true;
    }
    cout << "DataPreparation: введення завершено.\n";
}

void DataProcessing() {
    unique_lock<mutex> lock(dataMutex);
    while (!dataReady) {
        lock.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));
        lock.lock();
    }

    cout << "Прості числа з введеної черги: ";
    bool found = false;
    for (int value : dataQueue) {
        if (value >= 2 && isPrime(static_cast<uint64_t>(value))) {
            cout << value << ' ';
            found = true;
        }
    }
    if (!found) cout << "(простих чисел немає)";
    cout << '\n';
}
} 

void task1() {
    {
        lock_guard<mutex> lock(task121::dataMutex);
        task121::dataQueue.clear();
        task121::dataReady = false;
    }

    thread preparation(task121::DataPreparation);
    preparation.detach();
    task121::DataProcessing();

    
}

// Завдання 1.2.2
// Три потоки Waits очікують на i == 1. Awake надсилає два notify_all():
// перший не задовольняє умову, другий — після встановлення i = 1.

namespace task122 {
mutex m;
condition_variable cv;
int i = 0;

void Waits(int id) {
    unique_lock<mutex> lock(m);
    cout << "Waits " << id << ": входить у стан очікування.\n";
    cv.wait(lock, [] { return i == 1; });
    cout << "Waits " << id << ": очікування завершено.\n";
}

void Awake() {
    this_thread::sleep_for(milliseconds(100));
    cout << "Awake: перше повідомлення notify_all().\n";
    cv.notify_all();

    this_thread::sleep_for(milliseconds(100));
    {
        lock_guard<mutex> lock(m);
        i = 1;
    }
    cout << "Awake: i = 1, повторне повідомлення notify_all().\n";
    cv.notify_all();
}
} 

void task2() {
    {
        lock_guard<mutex> lock(task122::m);
        task122::i = 0;
    }

    thread w1(task122::Waits, 1);
    thread w2(task122::Waits, 2);
    thread w3(task122::Waits, 3);
    thread awake(task122::Awake);

    awake.join();
    w1.join();
    w2.join();
    w3.join();
}

// Завдання 1.2.3
// notify_one() пробуджує одного очікувача. Після цього головний потік надсилає
// notify_all() лише для завершення решти потоків, щоб програма не зависла.

namespace task123 {
mutex m;
condition_variable cv;
int i = 0;

void Waits(int id) {
    unique_lock<mutex> lock(m);
    cv.wait(lock, [] { return i == 1; });
    cout << "Повідомлення з потоку " << id << ".\n";
}

void Notify() {
    this_thread::sleep_for(milliseconds(100));
    {
        lock_guard<mutex> lock(m);
        i = 1;
    }
    cout << "Notify: викликається notify_one().\n";
    cv.notify_one();
}
} 

void task3() {
    {
        lock_guard<mutex> lock(task123::m);
        task123::i = 0;
    }

    thread t1(task123::Waits, 1);
    thread t2(task123::Waits, 2);
    thread t3(task123::Waits, 3);
    thread notifier(task123::Notify);

    notifier.join();

    this_thread::sleep_for(milliseconds(100));
    cout << "Головний потік: notify_all() для завершення решти потоків.\n";
    task123::cv.notify_all();

    t1.join();
    t2.join();
    t3.join();
}

// Завдання 1.2.4
// Синхронізація через condition_variable; предикат перевіряє наявність даних
// у черзі, а inputFinished дозволяє коректно завершити обробку після EOF/q.

namespace task124 {
deque<int> dataQueue;
mutex m;
condition_variable cv;
bool inputFinished = false;

void DataPreparation() {
    cout << "Введіть цілі числа (для завершення введіть q):\n";
    string token;

    while (cin >> token) {
        if (token == "q" || token == "Q") break;

        try {
            size_t parsed = 0;
            long long value = stoll(token, &parsed);
            if (parsed != token.size() ||
                value < numeric_limits<int>::min() ||
                value > numeric_limits<int>::max()) {
                cout << "Пропущено некоректне число: " << token << '\n';
                continue;
            }
            {
                lock_guard<mutex> lock(m);
                dataQueue.push_back(static_cast<int>(value));
            }
            cv.notify_one();
        } catch (...) {
            cout << "Пропущено некоректне число: " << token << '\n';
        }
    }

    {
        lock_guard<mutex> lock(m);
        inputFinished = true;
    }
    cv.notify_one();
}

void DataProcessing() {
    cout << "Прості числа з введеної черги: ";
    bool found = false;

    while (true) {
        int value;
        {
            unique_lock<mutex> lock(m);
            cv.wait(lock, [] { return !dataQueue.empty() || inputFinished; });

            if (dataQueue.empty() && inputFinished) break;

            value = dataQueue.front();
            dataQueue.pop_front();
        }

        if (value >= 2 && isPrime(static_cast<uint64_t>(value))) {
            cout << value << ' ';
            found = true;
        }
    }

    if (!found) cout << "(простих чисел немає)";
    cout << '\n';
}
} 

void task4() {
    {
        lock_guard<mutex> lock(task124::m);
        task124::dataQueue.clear();
        task124::inputFinished = false;
    }

    thread processing(task124::DataProcessing);
    thread preparation(task124::DataPreparation);

    preparation.join();
    processing.join();
}

// Завдання 1.2.5
// Порівняння std::launch::deferred та std::launch::async.
// Для коректного порівняння вимірюється час від get() до отримання результату.

void task5() {
    size_t n;
    if (!readPositiveN("Введіть n для пошуку n-го простого числа (1..1000000): ", n))
        return;

    cout << "Оберіть функцію від n: 1 — sqrt(n), 2 — sin(n), 3 — ln(n): ";
    int choice;
    if (!(cin >> choice) || choice < 1 || choice > 3) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Некоректний вибір функції.\n";
        return;
    }

    // У режимі deferred обчислення простого числа не починається до get().
    auto deferred = async(launch::deferred, nthPrime, n);
    cout << "Результат вибраної функції: ";
    if (choice == 1) cout << sqrt(static_cast<double>(n));
    else if (choice == 2) cout << sin(static_cast<double>(n));
    else cout << log(static_cast<double>(n));
    cout << "\nОтримання результату deferred...\n";

    auto startDeferred = steady_clock::now();
    uint64_t primeDeferred = deferred.get();
    auto endDeferred = steady_clock::now();
    auto deferredMs = duration_cast<milliseconds>(endDeferred - startDeferred).count();

    cout << "n-те просте число (deferred): " << primeDeferred << '\n';
    cout << "Час очікування get() для deferred: " << deferredMs << " мс\n\n";

    // У режимі async обчислення виконується паралельно з наступними діями.
    auto asynchronous = async(launch::async, nthPrime, n);
    cout << "Результат вибраної функції: ";
    if (choice == 1) cout << sqrt(static_cast<double>(n));
    else if (choice == 2) cout << sin(static_cast<double>(n));
    else cout << log(static_cast<double>(n));
    cout << "\nОтримання результату async...\n";

    auto startAsync = steady_clock::now();
    uint64_t primeAsync = asynchronous.get();
    auto endAsync = steady_clock::now();
    auto asyncMs = duration_cast<milliseconds>(endAsync - startAsync).count();

    cout << "n-те просте число (async): " << primeAsync << '\n';
    cout << "Час очікування get() для async: " << asyncMs << " мс\n";
    cout << "Висновок: deferred виконує завдання під час get(), а async запускає "
            "його в окремому потоці. Тому час очікування async залежить від того, "
            "скільки обчислень завершилося до виклику get().\n";
}

// Завдання 1.2.6
// Дві deque захищені одним mutex. Worker дістає packaged_task та відповідний n.
// future зберігаються в головному потоці для отримання результатів.

void task6() {
    using PrimeTask = packaged_task<uint64_t()>;

    deque<PrimeTask> tasks;
    deque<size_t> numbers;
    mutex m;
    condition_variable cv;
    bool stopWorker = false;
    vector<pair<size_t, future<uint64_t>>> results;

    thread worker([&] {
        while (true) {
            PrimeTask currentTask;
            {
                unique_lock<mutex> lock(m);
                cv.wait(lock, [&] { return stopWorker || !tasks.empty(); });

                if (tasks.empty() && stopWorker) break;

                currentTask = move(tasks.front());
                tasks.pop_front();
                numbers.pop_front();
            }
            currentTask();
        }
    });

    cout << "Вводьте n для обчислення n-го простого числа (q — завершити).\n";
    while (true) {
        cout << "n = ";
        string token;
        if (!(cin >> token)) break;
        if (token == "q" || token == "Q") break;

        size_t n = 0;
        try {
            size_t parsed = 0;
            unsigned long long value = stoull(token, &parsed);
            if (parsed != token.size() || value == 0 || value > 1000000) {
                cout << "Введіть число від 1 до 1000000 або q.\n";
                continue;
            }
            n = static_cast<size_t>(value);
        } catch (...) {
            cout << "Некоректне введення. Введіть число або q.\n";
            continue;
        }

        PrimeTask task([n] { return nthPrime(n); });
        future<uint64_t> result = task.get_future();

        {
            lock_guard<mutex> lock(m);
            tasks.push_back(move(task));
            numbers.push_back(n);
        }
        results.emplace_back(n, move(result));
        cv.notify_one();
    }

    {
        lock_guard<mutex> lock(m);
        stopWorker = true;
    }
    cv.notify_one();
    worker.join();

    cout << "\nРезультати:\n";
    for (auto& item : results) {
        cout << item.first << "-е просте число = " << item.second.get() << '\n';
    }
}

// Завдання 1.2.7
// Глобальні змінні не використовуються. Перший потік повертає два простих числа
// через promise/future та сигналізує другому потоку через promise<bool>.

void firstPromiseWorker(size_t n,
                        promise<uint64_t> primeNPromise,
                        promise<bool> startSignalPromise,
                        promise<uint64_t> prime10NPromise) {
    try {
        primeNPromise.set_value(nthPrime(n));
        startSignalPromise.set_value(true);
        prime10NPromise.set_value(nthPrime(n * 10));
    } catch (...) {
        auto error = current_exception();
        try { primeNPromise.set_exception(error); } catch (...) {}
        try { startSignalPromise.set_exception(error); } catch (...) {}
        try { prime10NPromise.set_exception(error); } catch (...) {}
    }
}

void secondPromiseWorker(size_t n, future<bool> startSignal, mutex& outputMutex) {
    try {
        if (startSignal.get()) {
            this_thread::sleep_for(seconds(2));
            cout << "\nДругий потік: sqrt(n) = " << sqrt(static_cast<double>(n)) << '\n';
        }
    } catch (const exception& e) {
        lock_guard<mutex> lock(outputMutex);
        cerr << "Помилка другого потоку: " << e.what() << '\n';
    }
}

void task7() {
    size_t n;
    if (!readPositiveN("Введіть n (1..100000): ", n, 100000)) return;

    promise<uint64_t> primeNPromise;
    promise<bool> startSignalPromise;
    promise<uint64_t> prime10NPromise;

    future<uint64_t> primeNFuture = primeNPromise.get_future();
    future<bool> startSignalFuture = startSignalPromise.get_future();
    future<uint64_t> prime10NFuture = prime10NPromise.get_future();

    mutex outputMutex;
    thread first(firstPromiseWorker, n,
                 move(primeNPromise),
                 move(startSignalPromise),
                 move(prime10NPromise));
    thread second(secondPromiseWorker, n, move(startSignalFuture), ref(outputMutex));

    try {
        cout << n << "-е просте число = " << primeNFuture.get() << '\n';
        cout << (n * 10) << "-е просте число = " << prime10NFuture.get() << '\n';
    } catch (const exception& e) {
        cerr << "Помилка обчислення: " << e.what() << '\n';
    }

    first.join();
    second.join();
    cout << "Обидва робочі потоки завершилися.\n";
}


int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    cout << "Лабораторна робота № 2\n"
         << "Синхронізація паралельних операцій та майбутніх результатів\n\n";

    while (true) {
        cout << "\nОберіть завдання:\n"
             << "1 — 1.2.1: прапорець і sleep_for\n"
             << "2 — 1.2.2: condition_variable та notify_all\n"
             << "3 — 1.2.3: condition_variable та notify_one\n"
             << "4 — 1.2.4: черга та condition_variable\n"
             << "5 — 1.2.5: std::async (deferred / async)\n"
             << "6 — 1.2.6: std::packaged_task та робочий потік\n"
             << "7 — 1.2.7: std::promise / std::future\n"
             << "0 — завершити програму\n"
             << "Ваш вибір: ";

        int choice;
        if (!(cin >> choice)) {
            if (cin.eof()) break;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Введіть номер від 0 до 7.\n";
            continue;
        }

        try {
            switch (choice) {
                case 1: task1(); break;
                case 2: task2(); break;
                case 3: task3(); break;
                case 4: task4(); break;
                case 5: task5(); break;
                case 6: task6(); break;
                case 7: task7(); break;
                case 0:
                    cout << "Завершення програми.\n";
                    return 0;
                default:
                    cout << "Невірний номер. Оберіть від 0 до 7.\n";
            }
        } catch (const exception& e) {
            cerr << "Помилка: " << e.what() << '\n';
        }
    }

    return 0;
}

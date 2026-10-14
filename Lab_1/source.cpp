#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <string>
#include <algorithm>
#include <condition_variable>
#include <limits>
#include <utility>
#include <windows.h>

using namespace std;

// Спільні допоміжні засоби

mutex coutMutex;
list<int> l;
mutex listMutex;

// Дозволяє дочекатися завершення від'єднаних потоків,
// не замінюючи detach() на join().
class CompletionCounter {
private:
    mutex m;
    condition_variable cv;
    int completed = 0;

public:
    void done() {
        {
            lock_guard<mutex> lock(m);
            ++completed;
        }
        cv.notify_one();
    }

    void waitFor(int total) {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&] { return completed >= total; });
    }
};

void printLine(const string& text) {
    lock_guard<mutex> lock(coutMutex);
    cout << text << '\n';
}

int readInt(const string& prompt) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            return value;
        }

        cout << "Помилка введення. Введіть ціле число.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// 1.2.1 та 1.2.2 — створення потоків і detach()

void Thread1() {
    printLine("Thread1: 1");
}

void Thread2() {
    printLine("Thread2: 2");
}

// Ця функція навмисно демонструє саме умову 1.2.1.
// У стандартному C++ деструктор joinable std::thread викликає
// std::terminate(). Тому після запуску цього пункту програма
// аварійно завершується — це очікуваний наслідок відсутності
// і join(), і detach(), а не помилка компіляції.
void demo121() {
    cout << "\n1.2.1: потоки без join() і detach().\n";
    cout << "За стандартом C++ деструктор joinable-потоку викликає "
            "std::terminate(). Цей пункт завершує програму.\n";

    thread t1(Thread1);
    thread t2(Thread2);

    // Навмисно не викликаємо ні join(), ні detach().
    // Вихід із функції призведе до std::terminate().
}

void demo122() {
    cout << "\n1.2.2: потоки з detach().\n";

    thread t1(Thread1);
    thread t2(Thread2);

    t1.detach();
    t2.detach();

    cout << "Головний потік продовжив виконання, не очікуючи "
            "від'єднані потоки.\n";
    cout << "Порядок виведення та наявність обох повідомлень "
            "не гарантовані.\n";
}

// 1.2.3 — робота зі списком без синхронізації 

void AddToList(int startValue) {
    for (int i = 0; i < 10; ++i) {
        int value = startValue + i;
        l.push_back(value);

        lock_guard<mutex> outputLock(coutMutex);
        cout << "AddToList: додано елемент " << value << '\n';
    }
}

void ListContains(int value) {
    for (int i = 0; i < 10; ++i) {
        // Навмисно немає listMutex: це демонстрація пункту 1.2.3.
        bool found = (find(l.begin(), l.end(), value) != l.end());

        lock_guard<mutex> outputLock(coutMutex);
        cout << "ListContains: число " << value
             << (found ? " входить" : " не входить")
             << " до списку (спроба " << i + 1 << ").\n";
    }
}

void demo123() {
    cout << "\n1.2.3: список без захисту м'ютексом.\n";
    cout << "Одночасне читання та модифікація std::list "
            "створюють гонку даних і невизначену поведінку.\n";

    l.clear();
    int value = readInt("Введіть початкове число: ");

    thread t1(AddToList, value);
    thread t2(ListContains, value);

    t1.join();
    t2.join();

    cout << "Розмір списку після завершення потоків: "
         << l.size() << '\n';
}

// 1.2.4 — захист списку прямим використанням mutex

void AddToListProtected(int startValue) {
    for (int i = 0; i < 10; ++i) {
        int value = startValue + i;

        {
            lock_guard<mutex> lock(listMutex);
            l.push_back(value);
        }

        lock_guard<mutex> outputLock(coutMutex);
        cout << "AddToList: успішно додано " << value << '\n';
    }
}

void ListContainsProtected(int value) {
    for (int i = 0; i < 10; ++i) {
        bool found;

        {
            lock_guard<mutex> lock(listMutex);
            found = (find(l.begin(), l.end(), value) != l.end());
        }

        lock_guard<mutex> outputLock(coutMutex);
        cout << "ListContains: число " << value
             << (found ? " входить" : " не входить")
             << " до списку (спроба " << i + 1 << ").\n";
    }
}

void demo124() {
    cout << "\n1.2.4: список захищений м'ютексом.\n";

    l.clear();
    int value = readInt("Введіть початкове число: ");

    thread t1(AddToListProtected, value);
    thread t2(ListContainsProtected, value);

    t1.join();
    t2.join();

    cout << "Розмір списку після завершення потоків: "
         << l.size() << '\n';
}

// 1.2.5 — по 10 від'єднаних потоків, lock_guard

void AddOneToList(int value) {
    {
        lock_guard<mutex> lock(listMutex);
        l.push_back(value);
    }

    lock_guard<mutex> outputLock(coutMutex);
    cout << "AddToList: додано " << value << '\n';
}

void CheckOneInList(int value) {
    bool found;

    {
        lock_guard<mutex> lock(listMutex);
        found = (find(l.begin(), l.end(), value) != l.end());
    }

    lock_guard<mutex> outputLock(coutMutex);
    cout << "ListContains: число " << value
         << (found ? " входить" : " не входить")
         << " до списку.\n";
}

void demo125() {
    cout << "\n1.2.5: 10 потоків додавання та 10 потоків перевірки.\n";
    cout << "Кожен потік виконує одну операцію; використовується "
            "lock_guard та detach().\n";

    l.clear();
    const int startValue = readInt("Введіть початкове число: ");
    const int searchValue = startValue;
    CompletionCounter completion;

    for (int i = 0; i < 10; ++i) {
        thread t([&, value = startValue + i] {
            AddOneToList(value);
            completion.done();
        });
        t.detach();
    }

    for (int i = 0; i < 10; ++i) {
        thread t([&] {
            CheckOneInList(searchValue);
            completion.done();
        });
        t.detach();
    }

    // Потоки від'єднані, але main чекає завершення всіх 20 операцій,
    // щоб не знищити спільні дані завчасно.
    completion.waitFor(20);

    cout << "Усі від'єднані потоки завершили операції.\n";
    cout << "Розмір списку: " << l.size() << '\n';
}

// 1.2.6 та 1.2.7 — класи someData і exchangePerson

class someData {
public:
    string name;
    string surname;
    string address;
    int age;

    someData()
        : name(""), surname(""), address(""), age(0) {}

    someData(string n, string s, string a, int ag)
        : name(std::move(n)),
          surname(std::move(s)),
          address(std::move(a)),
          age(ag) {}
};

void printPersonData(const string& label, const someData& data) {
    cout << label
         << "Ім'я: " << data.name
         << ", прізвище: " << data.surname
         << ", адреса: " << data.address
         << ", вік: " << data.age << '\n';
}

class exchangePerson {
private:
    someData data;
    mutex m;

public:
    explicit exchangePerson(const someData& initialData)
        : data(initialData) {}

    exchangePerson(const exchangePerson&) = delete;
    exchangePerson& operator=(const exchangePerson&) = delete;

    static void JohnDoe(exchangePerson& person) {
        lock_guard<mutex> lock(person.m);
        person.data.name = "John";
        person.data.surname = "Doe";
        person.data.address = "Unknown";
        person.data.age = 120;
    }

    static void JacobSmith(exchangePerson& person) {
        lock_guard<mutex> lock(person.m);
        person.data.name = "Jacob";
        person.data.surname = "Smith";
        person.data.address = "Known";
        person.data.age = 1;
    }

    
    // 1.2.6 std::lock + lock_guard із adopt_lock.
    static void SwapWithLockGuard(exchangePerson& first,
                                  exchangePerson& second) {
        if (&first == &second) {
            printLine("Swap: передано той самий об'єкт; обмін не виконано.");
            return;
        }

        std::lock(first.m, second.m);
        lock_guard<mutex> lock1(first.m, adopt_lock);
        lock_guard<mutex> lock2(second.m, adopt_lock);

        {
            lock_guard<mutex> outputLock(coutMutex);
            cout << "\nДані до обміну:\n";
            printPersonData("Перший об'єкт: ", first.data);
            printPersonData("Другий об'єкт: ", second.data);
        }

        swap(first.data, second.data);

        {
            lock_guard<mutex> outputLock(coutMutex);
            cout << "Дані після обміну:\n";
            printPersonData("Перший об'єкт: ", first.data);
            printPersonData("Другий об'єкт: ", second.data);
        }
    }

    //1.2.7 unique_lock із defer_lock і спільний std::lock.
    static void SwapWithUniqueLock(exchangePerson& first,
                                   exchangePerson& second) {
        if (&first == &second) {
            printLine("Swap: передано той самий об'єкт; обмін не виконано.");
            return;
        }

        unique_lock<mutex> lock1(first.m, defer_lock);
        unique_lock<mutex> lock2(second.m, defer_lock);
        std::lock(lock1, lock2);

        {
            lock_guard<mutex> outputLock(coutMutex);
            cout << "\nДані до обміну:\n";
            printPersonData("Перший об'єкт: ", first.data);
            printPersonData("Другий об'єкт: ", second.data);
        }

        swap(first.data, second.data);

        {
            lock_guard<mutex> outputLock(coutMutex);
            cout << "Дані після обміну:\n";
            printPersonData("Перший об'єкт: ", first.data);
            printPersonData("Другий об'єкт: ", second.data);
        }
    }
};

void demoPersonExchange(bool useUniqueLock) {
    cout << (useUniqueLock
        ? "\n1.2.7: обмін із unique_lock та defer_lock.\n"
        : "\n1.2.6: обмін із lock_guard та adopt_lock.\n");

    exchangePerson first(someData("Initial1", "Person1", "Address1", 20));
    exchangePerson second(someData("Initial2", "Person2", "Address2", 30));

    CompletionCounter completion;

    // Для кожного об'єкта запускаємо JohnDoe та JacobSmith
    // в окремих від'єднаних потоках.
    thread t1([&] {
        exchangePerson::JohnDoe(first);
        completion.done();
    });
    t1.detach();

    thread t2([&] {
        exchangePerson::JacobSmith(first);
        completion.done();
    });
    t2.detach();

    thread t3([&] {
        exchangePerson::JohnDoe(second);
        completion.done();
    });
    t3.detach();

    thread t4([&] {
        exchangePerson::JacobSmith(second);
        completion.done();
    });
    t4.detach();

    // Обмін виконується в окремому потоці. join() гарантує,
    // що Swap завершиться до продовження main.
    thread swapThread([&] {
        if (useUniqueLock) {
            exchangePerson::SwapWithUniqueLock(first, second);
        } else {
            exchangePerson::SwapWithLockGuard(first, second);
        }
    });

    swapThread.join();

    // Додатково чекаємо на всі 4 від'єднані потоки, щоб об'єкти
    // first і second не були знищені, поки потоки їх використовують.
    completion.waitFor(4);
    cout << "Усі потоки модифікації завершили роботу.\n";
}

// Меню

void printMenu() {
    cout << "\n============================================\n";
    cout << "Лабораторна робота №1\n";
    cout << "М'ютекси та потоки в C++\n";
    cout << "============================================\n";
    cout << "1.2.1 — потоки без join() і detach() [завершити програму]\n";
    cout << "1.2.2 — потоки з detach()\n";
    cout << "1.2.3 — список без синхронізації (гонка даних)\n";
    cout << "1.2.4 — список із захистом м'ютексом\n";
    cout << "1.2.5 — 10 + 10 від'єднаних потоків, lock_guard\n";
    cout << "1.2.6 — обмін даними, lock_guard + adopt_lock\n";
    cout << "1.2.7 — обмін даними, unique_lock + defer_lock\n";
    cout << "0. Вихід\n";
}

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    while (true) {
        printMenu();
        int choice = readInt("Оберіть пункт: ");

        switch (choice) {
        case 1:
            demo121();
            // Фактично std::terminate() відбудеться під час
            // знищення joinable thread; виконання сюди зазвичай не повернеться.
            break;
        case 2:
            demo122();
            break;
        case 3:
            demo123();
            break;
        case 4:
            demo124();
            break;
        case 5:
            demo125();
            break;
        case 6:
            demoPersonExchange(false);
            break;
        case 7:
            demoPersonExchange(true);
            break;
        case 0:
            cout << "Завершення програми.\n";
            return 0;
        default:
            cout << "Невідомий пункт меню. Спробуйте ще раз.\n";
            break;
        }
    }
}

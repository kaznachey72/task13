# Задание
Написать HTTP-сервер на основе одного из современных механизмов мультиплексирования ввода-вывода
(epoll, kqueue), раздающий файлы из заданной директории. Сторонние библиотеки не использовать.

# Сложность
★★★★☆

# Цель задания
Получить опыт создания асинхронных серверов.

# Критерии успеха
1. Создано приложение, принимающее аргументами командной строки рабочую директорию и пару адрес:порт для прослушивания.
2. Сервер корректно отдаёт файлы из заданной директории по HTTP.
3. Сервер корректно отвечает 404 статус-кодом на запросы несуществующих файлов и 403 статус-кодом на запросы файлов, на чтение которых у процесса не хватает прав.
4. Бонусные баллы за решение, обеспечивающее наибольшее RPS.
5. Код компилируется без предупреждений с ключами компилятора -Wall -Wextra -Wpedantic -std=c11.
6. Далее успешность определяется ревью кода.
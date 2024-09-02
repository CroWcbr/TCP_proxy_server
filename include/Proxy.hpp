#pragma once

#include "../include/Logger.hpp"
#include "../include/Logger_query.hpp"
#include <string>
#include <vector>
#include <map>

#include <poll.h>

constexpr const char* LOG_QUERY = "log_query";      ///< Имя файла логов запросов.
constexpr const char* LOG_DEBUG = "log_debug";      ///< Имя файла логов отладки.
constexpr const char* PROXY_HOST = "127.0.0.1";     ///< Хост для прокси.
constexpr int MAX_LISTEN = 100;                     ///< Максимальное количество ожидающих соединений.
constexpr int MAX_BUFFER_RECV = 65536;              ///< Максимальный размер буфера для приема данных. 64Кб.

/**
 * @struct Connection
 * @brief Структура, представляющая соединение в прокси-сервере.
 */
struct Connection
{
    int                 to;         ///< FD сервера, с которым связан клиент.
    bool                client;     ///< Флаг, указывающий, является ли соединение клиентом.
    bool                active;     ///< Флаг активности соединения.
    int                 len_query;  ///< Длина SQL запроса.
    std::vector<char>   data;       ///< Вектор данных запроса.
};

/**
 * @class Proxy
 * @brief Класс прокси-сервера.
 */
class Proxy
{
private:
// Логгеры
    Logger_query    log_query;  ///< Логгер запросов.
    Logger          log_debug;  ///< Логгер отладки.

// вектор открытых соединений
    typedef std::vector<struct pollfd> pollfdType;  ///< Тип для хранения дескрипторов файлов.
    pollfdType      fds;                            ///< Вектор дескрипторов файлов.
    std::map<int, Connection> connection;           ///< Словарь соединений.

// Параметры прокси-сервера
    std::string     proxy_host;         ///< Хост прокси-сервера.
    std::string     proxy_port;         ///< Порт прокси-сервера.
    int             proxy_fd;           ///< Дескриптор сокета прокси-сервера.

// Параметры PostgreSQL
    std::string     postgresql_host;    ///< Хост PostgreSQL.
    std::string     postgresql_port;    ///< Порт PostgreSQL.

// Точка остановки
    static bool     should_stop;        ///< Флаг остановки сервера.

private:
    /**
     * @brief Инициализация параметров из командной строки.
     * @param argc Количество аргументов.
     * @param argv Массив аргументов.
     * @return true, если параметры инициализированы успешно; false в противном случае.
     */
    bool    _init_param(int argc, char **argv);

    /**
     * @brief Инициализирует и запускает прокси-сервер.
     * 
     * Этот метод настраивает сокет для прокси-сервера, используя предоставленные параметры
     * хоста и порта. Он выполняет следующие действия:
     * - Получает адресную информацию с помощью функции `getaddrinfo`.
     * - Создает сокет для каждого из возможных адресов.
     * - Настраивает сокет (включает повторное использование адреса).
     * - Привязывает сокет к адресу.
     * - Устанавливает сокет в неблокирующий режим.
     * - Начинает прослушивание входящих соединений.
     * 
     * @return
     * Возвращает `true`, если все шаги выполнены успешно и сервер готов к работе. В противном случае
     * возвращает `false` и записывает сообщение об ошибке в лог.
     * 
     * @details
     * - Функция `getaddrinfo` используется для получения списка адресов, к которым можно привязать
     *   сокет. Если возникает ошибка, она логируется и метод возвращает `false`.
     * - В цикле перебираются все возможные адреса. Для каждого адреса создается сокет и настраивается
     *   с помощью `setsockopt`, чтобы включить повторное использование адреса.
     * - После успешного привязывания сокета к адресу цикл прерывается. Если не удается привязать
     *   сокет ни к одному адресу, записывается сообщение об ошибке и метод возвращает `false`.
     * - Устанавливается неблокирующий режим для сокета с помощью `fcntl`. Если это не удается,
     *   сокет закрывается и возвращается `false`.
     * - Наконец, метод начинает прослушивание входящих соединений с помощью `listen`. Если
     *   происходит ошибка, сокет закрывается и метод возвращает `false`.
     */  
    bool    _proxy_start();

   /**
 * @brief Обрабатывает входящее соединение на сервере прокси.
 * 
 * Этот метод вызывается, когда сервер прокси обнаруживает входящее соединение. Он выполняет
 * следующие действия:
 * - Принимает входящее соединение.
 * - Настраивает новый дескриптор сокета клиента и устанавливает его в неблокирующий режим.
 * - Создает сокет для подключения к удаленному серверу и также устанавливает его в неблокирующий режим.
 * - Добавляет новые дескрипторы сокетов в список дескрипторов для мониторинга.
 * - Сохраняет информацию о соединениях в контейнерах для последующего управления.
 * 
 * @param[in,out] it Итератор на элемент вектора дескрипторов `fds`, который указывает на
 * сокет сервера прокси. Итератор будет обновлен, чтобы отразить состояние событий.
 * 
 * @details
 * - Сначала метод очищает поле `revents` у текущего дескриптора, чтобы подготовить его для
 *   обработки новых событий.
 * - Принимается новое соединение с помощью `accept`. Если возникает ошибка, она логируется
 *   и метод завершает выполнение.
 * - Новый дескриптор клиента настраивается на неблокирующий режим с помощью `fcntl`. Если
 *   настройка не удалась, дескриптор закрывается и метод завершает выполнение.
 * - Получается адресная информация для подключения к удаленному серверу с помощью `getaddrinfo`.
 *   Если возникает ошибка, она логируется, и дескриптор клиента закрывается.
 * - Создается сокет для подключения к удаленному серверу. Если сокет создать не удалось,
 *   логируется ошибка, дескриптор клиента закрывается, и адресная информация освобождается.
 * - Подключение к удаленному серверу выполняется с помощью `connect`. Если подключение не удается,
 *   ошибки логируются, и оба сокета закрываются.
 * - После успешного подключения и настройки оба дескриптора (клиентский и удаленный) добавляются
 *   в список дескрипторов для мониторинга.
 * - Информация о соединениях сохраняется в контейнере `connection` для дальнейшего управления.
 */
    void    _poll_in_serv(pollfdType::iterator &it);

    /**
     * @brief Обрабатывает входящие данные от клиента или сервера.
     * 
     * Этот метод вызывается, когда обнаружены данные для чтения из сокета, связанного с клиентом или сервером.
     * Он выполняет следующие действия:
     * - Принимает данные из сокета.
     * - Обрабатывает ошибки и завершение соединения.
     * - Логирует запросы, если данные получены от клиента и является запросом.
     * - Пересылает данные от клиента к серверу и наоборот.
     * 
     * @param[in,out] it Итератор на элемент вектора дескрипторов `fds`, который указывает на
     * сокет, из которого читаются данные. Итератор будет обновлен, чтобы отразить состояние событий.
     * 
     * @details
     * - Метод сначала читает данные из сокета в буфер `buffer`.
     * - Если произошла ошибка при чтении данных, обрабатываются разные виды ошибок:
     *   - `EAGAIN` и `EWOULDBLOCK`: Операция завершилась с ошибкой, так как сокет является неблокирующим.
     *   - `EINTR`: Операция была прервана сигналом.
     *   - `ECONNRESET` и `EPIPE`: Соединение было сброшено или разорвано.
     * - Если `recv` возвращает 0, это означает, что соединение закрыто.
     * - Если данные успешно прочитаны, и это запрос от клиента (определяется по первому байту 'Q'), метод
     *   обрабатывает и сохраняет запрос.
     * - Если запрос не полный он дописывается в буфер.
     * - Полученные данные отправляются на сервер и наоборот.
     * - При отправке данных проверяется количество отправленных байтов и повторяется попытка, если
     *   сокет неблокирующий и возникла ошибка.
     */
    void    _poll_in_connection(pollfdType::iterator &it);

    /**
     * @brief Обработка события отправки сообщения. Не требуется, реализация отсутствует.
     * @param it Итератор struct pollfd для обработки события.
     */
    void    _poll_out(pollfdType::iterator &it);

    /**
     * @brief Обработка других событий.
     * @param it Итератор struct pollfd для обработки события.
     */
    void    _poll_else(pollfdType::iterator &it);

public:
    /**
     * @brief Конструктор по умолчанию удален.
     */
    Proxy() = delete;

    /**
     * @brief Конструктор класса Proxy.
     * @param argc Количество аргументов.
     * @param argv Массив аргументов.
     */
    Proxy(int argc, char **argv);

    /// @brief Запрещает копирование прокси.
    Proxy(Proxy const &copy) = delete;

    /// @brief Запрещает присваивание прокси копии.
    Proxy &operator=(Proxy const &copy) = delete;

    /// @brief Деструктор. Закрывает все соединения.
    ~Proxy();

    /// @brief Установка флага остановки сервера.
    static void stop() { should_stop = true; }

    /**
     * @brief Запуск основного цикла работы прокси-сервера.
     * 
     * Этот метод запускает основной цикл обработки событий для прокси-сервера. Внутри цикла метод
     * использует функцию `poll` для ожидания событий на дескрипторах файлов, зарегистрированных
     * в векторе `fds`. В зависимости от типа события, обрабатываются новые входящие соединения,
     * входящие и исходящие данные, а также другие события. После обработки всех событий,
     * отключенные соединения удаляются.
     * 
     * Цикл продолжается до тех пор, пока не будет установлен флаг остановки `should_stop`.
     * 
     * @details
     * - Сначала метод вызывает `poll` для ожидания событий на дескрипторах файлов.
     * - Если функция `poll` возвращает ошибку, выводится предупреждающее сообщение в лог.
     * - Если функция `poll` возвращает 0, это означает, что время ожидания истекло, и цикл продолжает работу.
     * - В цикле обработки событий:
     *   - Если событие относится к новому входящему соединению на основном сокете `proxy_fd`,
     *     вызывается метод `_poll_in_serv` для обработки нового соединения.
     *   - Если событие относится к существующему соединению, обрабатываются входящие данные,
     *     исходящие данные или другие события, в зависимости от значения флагов `revents`.
     * - После обработки всех событий происходит удаление отключенных клиентов:
     *   - Закрываются и удаляются дескрипторы файлов для отключенных соединений.
     *   - Удаляются соответствующие записи из контейнера `connection` и вектора `fds`.
     * 
     * @note
     * Отключенные клиенты должны быть корректно отмечены как неактивные.
     */
    void run();
};



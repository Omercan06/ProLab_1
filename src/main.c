#include "prolab.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define INPUT_BUFFER_SIZE 512

#define COLOR_RESET "\x1b[0m"
#define COLOR_CYAN "\x1b[96m"
#define COLOR_GREEN "\x1b[92m"
#define COLOR_YELLOW "\x1b[93m"

static bool terminal_color_enabled = true;

static void print_cell(const char *text, size_t width);

static void configure_console(void)
{
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    terminal_color_enabled = false;
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode) &&
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        terminal_color_enabled = true;
    }
#endif
}

static void set_color(const char *color_code)
{
    if (terminal_color_enabled) {
        fputs(color_code, stdout);
    }
}

static void print_section(const char *title)
{
    set_color(COLOR_CYAN);
    puts("\n+======================================================================+");
    printf("| ");
    print_cell(title, 68);
    puts(" |");
    puts("+======================================================================+");
    set_color(COLOR_RESET);
}

static size_t utf8_column_count(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    size_t columns = 0;

    while (*cursor != '\0') {
        if ((*cursor & 0xC0U) != 0x80U) {
            ++columns;
        }
        ++cursor;
    }
    return columns;
}

static void print_cell(const char *text, size_t width)
{
    size_t columns = utf8_column_count(text);

    fputs(text, stdout);
    while (columns < width) {
        putchar(' ');
        ++columns;
    }
}

static bool read_line(const char *prompt, char *buffer, size_t capacity)
{
    size_t length;

    fputs(prompt, stdout);
    fflush(stdout);
    if (fgets(buffer, (int)capacity, stdin) == NULL) {
        return false;
    }
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    } else if (!feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Discard the rest of an overlong console line. */
        }
    }
    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    }
    return true;
}

static bool parse_integer(const char *text, int *value)
{
    char *end;
    long parsed;

    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (*text == '\0') {
        return false;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool read_integer(const char *prompt, int *value)
{
    char line[INPUT_BUFFER_SIZE];

    for (;;) {
        if (!read_line(prompt, line, sizeof(line))) {
            return false;
        }
        if (parse_integer(line, value)) {
            return true;
        }
        puts("Geçersiz giriş. Bir tam sayı girin.");
    }
}

static void print_technician_number(int index)
{
    if (index < 0) {
        printf("-");
    } else {
        printf("T%d", index + 1);
    }
}

static void print_task_row(const Task *task)
{
    char technician[16];

    printf("| %-5d | %-5d | %-5d | %-7d | ", task->id, task->arrival_time,
           task->total_duration, task->priority);
    print_cell(task_state_name(task->state), 15);
    printf(" | ");
    if (task->current_technician < 0) {
        strcpy(technician, "-");
    } else {
        (void)snprintf(technician, sizeof(technician), "T%d",
                       task->current_technician + 1);
    }
    print_cell(technician, 10);
    printf(" | %-6d |\n", task->remaining_duration);
}

static void list_tasks(const Simulation *simulation, int state_filter)
{
    size_t i;
    size_t count = 0;

    print_section(state_filter < 0 ? "GÖREV LİSTESİ" : "DURUMA GÖRE GÖREV LİSTESİ");
    puts("+-------+-------+-------+---------+-----------------+------------+--------+");
    printf("| ");
    print_cell("ID", 5);
    printf(" | ");
    print_cell("Geliş", 5);
    printf(" | ");
    print_cell("Süre", 5);
    printf(" | ");
    print_cell("Öncelik", 7);
    printf(" | ");
    print_cell("Durum", 15);
    printf(" | ");
    print_cell("Teknisyen", 10);
    printf(" | ");
    print_cell("Kalan", 6);
    puts(" |");
    puts("+-------+-------+-------+---------+-----------------+------------+--------+");
    for (i = 0; i < simulation->task_count; ++i) {
        const Task *task = &simulation->tasks[i];
        if (state_filter >= 0 && task->state != (TaskState)state_filter) {
            continue;
        }
        print_task_row(task);
        ++count;
    }
    puts("+-------+-------+-------+---------+-----------------+------------+--------+");
    printf("Gösterilen görev: %zu / %zu\n", count, simulation->task_count);
}

static const Task *find_task(const Simulation *simulation, int id)
{
    size_t i;
    for (i = 0; i < simulation->task_count; ++i) {
        if (simulation->tasks[i].id == id) {
            return &simulation->tasks[i];
        }
    }
    return NULL;
}

static void query_task(const Simulation *simulation)
{
    int id;
    const Task *task;

    if (!read_integer("Görev ID: ", &id)) {
        return;
    }
    task = find_task(simulation, id);
    if (task == NULL) {
        puts("Bu ID ile görev bulunamadı.");
        return;
    }
    print_section("GÖREV DETAYI");
    printf("ID: %d\nGeliş zamanı: %d\nToplam süre: %d\nÖncelik: %d\n"
           "Görev türü: %s\nKonum: %s\nDurum: %s\nTeknisyen: ",
           task->id, task->arrival_time, task->total_duration, task->priority,
           task->type, location_name(task->location), task_state_name(task->state));
    print_technician_number(task->current_technician);
    printf("\nKalan süre: %d\nİlk başlama zamanı: ", task->remaining_duration);
    if (task->first_start_time < 0) {
        printf("-\n");
    } else {
        printf("%d\n", task->first_start_time);
    }
    printf("Tamamlanma zamanı: ");
    if (task->completion_time < 0) {
        printf("-\n");
    } else {
        printf("%d\n", task->completion_time);
    }
    if (task->last_technician >= 0) {
        printf("Son bitiren teknisyen: T%d\n", task->last_technician + 1);
    }
}

static void show_technicians(const Simulation *simulation)
{
    size_t i;

    print_section("TEKNİSYEN DURUMLARI");
    puts("Teknisyen | Durum           | Görev ID | Kalan süre | Çalışılan");
    puts("-----------+-----------------+----------+------------+----------");
    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        const Technician *technician = &simulation->technicians[i];
        printf("T%-9zu | ", i + 1);
        set_color(technician->state == TECHNICIAN_IDLE ? COLOR_GREEN : COLOR_YELLOW);
        print_cell(technician_state_name(technician->state), 15);
        set_color(COLOR_RESET);
        if (technician->task_index >= 0) {
            const Task *task = &simulation->tasks[technician->task_index];
            printf(" | %8d | %10d | %9d\n", task->id,
                   task->remaining_duration, technician->worked_units);
        } else {
            printf(" | %8s | %10s | %9d\n", "-", "-", technician->worked_units);
        }
        if (technician->state == TECHNICIAN_IN_TRANSIT) {
            printf("  Konum geçişinde kalan: %d birim\n", technician->transition_remaining);
        }
        if (simulation->algorithm == ALGORITHM_ROUND_ROBIN &&
            technician->task_index >= 0) {
            printf("  Round Robin diliminde kalan: %d birim\n",
                   technician->quantum_remaining);
        }
    }
    puts("-----------+-----------------+----------+------------+----------");
}

static void show_statistics(const Simulation *simulation)
{
    size_t state_counts[4] = { 0, 0, 0, 0 };
    size_t completed = 0;
    size_t i;
    long long total_wait = 0;
    long long total_system = 0;
    long long total_worked = 0;
    int latest_completion = 0;

    for (i = 0; i < simulation->task_count; ++i) {
        const Task *task = &simulation->tasks[i];
        ++state_counts[task->state];
        if (task->state == TASK_COMPLETED) {
            ++completed;
            total_wait += (long long)task->completion_time - task->arrival_time -
                          task->total_duration;
            total_system += (long long)task->completion_time - task->arrival_time;
            if (task->completion_time > latest_completion) {
                latest_completion = task->completion_time;
            }
        }
    }
    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        total_worked += simulation->technicians[i].worked_units;
    }

    print_section("SİMÜLASYON İSTATİSTİKLERİ");
    printf("Toplam görev: %zu (Henüz gelmedi: %zu, Bekliyor: %zu, "
           "Devam ediyor: %zu, Tamamlandı: %zu)\n",
           simulation->task_count, state_counts[TASK_NOT_ARRIVED],
           state_counts[TASK_WAITING], state_counts[TASK_RUNNING],
           state_counts[TASK_COMPLETED]);
    if (completed == simulation->task_count) {
        set_color(COLOR_GREEN);
        printf("Makespan: %d\n", latest_completion);
        set_color(COLOR_RESET);
    } else {
        printf("Makespan: tüm görevler tamamlanmadı\n");
    }
    if (completed == 0) {
        printf("Ortalama bekleme: hesaplanamaz\nOrtalama sistemde kalma: hesaplanamaz\n");
    } else {
        printf("Ortalama bekleme: %.3f\nOrtalama sistemde kalma: %.3f\n",
               (double)total_wait / completed, (double)total_system / completed);
    }

    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        const Technician *technician = &simulation->technicians[i];
        printf("T%zu kullanım: ", i + 1);
        if (simulation->current_time == 0) {
            printf("hesaplanamaz\n");
        } else {
            printf("%.3f%%\n", 100.0 * technician->worked_units /
                                    simulation->current_time);
        }
    }
    printf("Genel kullanım: ");
    if (simulation->current_time == 0) {
        printf("hesaplanamaz\n");
    } else {
        printf("%.3f%%\n", 100.0 * (double)total_worked /
                                (PROLAB_TECHNICIAN_COUNT * (double)simulation->current_time));
    }
}

static bool select_algorithm(Simulation *simulation)
{
    int choice;
    int quantum = 0;
    SchedulingAlgorithm algorithm;

    puts("Algoritma seçin: 1-Shortest Job, 2-Priority, 3-Round Robin, 4-Özgün");
    if (!read_integer("Seçim: ", &choice)) {
        return false;
    }
    switch (choice) {
    case 1: algorithm = ALGORITHM_SHORTEST_JOB; break;
    case 2: algorithm = ALGORITHM_PRIORITY; break;
    case 3:
        algorithm = ALGORITHM_ROUND_ROBIN;
        if (!read_integer("Round Robin quantum (pozitif tam sayı): ", &quantum)) {
            return false;
        }
        if (quantum <= 0) {
            puts("Quantum pozitif olmalıdır.");
            return true;
        }
        break;
    case 4: algorithm = ALGORITHM_CUSTOM; break;
    default:
        puts("Algoritma seçimi geçersiz.");
        return true;
    }
    if (!simulation_start(simulation, algorithm, quantum)) {
        puts("Algoritma başlatılamadı.");
        return false;
    }
    printf("%s seçildi; simülasyon zaman 0'dan başlatıldı.\n",
           algorithm_name(algorithm));
    return true;
}

static void print_menu(const Simulation *simulation)
{
    print_section("TEKNİK DESTEK GÖREV ZAMANLAYICISI");
    printf("  Simülasyon zamanı: %d | Algoritma: %s", simulation->current_time,
           algorithm_name(simulation->algorithm));
    if (simulation->algorithm == ALGORITHM_ROUND_ROBIN) {
        printf(" | Quantum: %d", simulation->round_robin_quantum);
    }
    puts("\n+----------------------------------------------------------------------+");
    puts("|  [1] Algoritma seç/değiştir      [2] Tüm görevleri listele           |");
    puts("|  [3] Duruma göre listele         [4] ID ile görev sorgula            |");
    puts("|  [5] Teknisyenleri göster        [6] Zamanı 1 birim ilerlet          |");
    puts("|  [7] Zamanı N birim ilerlet      [8] Hedef zamana git                |");
    puts("|  [9] İstatistikleri göster       [10] Simülasyonu sıfırla            |");
    puts("|  [0] Çıkış                                                          |");
    puts("+----------------------------------------------------------------------+");
}

static bool load_tasks(Simulation *simulation)
{
    char path[INPUT_BUFFER_SIZE];
    TaskLoadReport report;

    for (;;) {
        if (!read_line("Görev dosyasının yolu: ", path, sizeof(path))) {
            return false;
        }
        if (path[0] == '\0') {
            puts("Dosya yolu boş olamaz.");
            continue;
        }
        if (task_file_load(path, simulation, &report, stderr)) {
            printf("Yükleme tamamlandı: %zu kabul, %zu ret, %zu atlanan satır.\n",
                   report.accepted_lines, report.rejected_lines, report.skipped_lines);
            return true;
        }
        puts("Dosya yüklenemedi. Başka bir yol deneyin; çıkmak için Ctrl+C kullanın.");
    }
}

int main(void)
{
    Simulation simulation;
    bool running = true;

    configure_console();
    if (!simulation_init(&simulation)) {
        fputs("Simülasyon belleği ayrılamadı.\n", stderr);
        return EXIT_FAILURE;
    }
    if (!load_tasks(&simulation)) {
        simulation_destroy(&simulation);
        return EXIT_FAILURE;
    }

    while (running) {
        int choice;
        print_menu(&simulation);
        if (!read_integer("İşlem: ", &choice)) {
            break;
        }
        switch (choice) {
        case 1:
            (void)select_algorithm(&simulation);
            break;
        case 2:
            list_tasks(&simulation, -1);
            break;
        case 3: {
            int state;
            puts("Durum: 1-Henüz gelmedi, 2-Bekliyor, 3-Devam ediyor, 4-Tamamlandı");
            if (read_integer("Seçim: ", &state) && state >= 1 && state <= 4) {
                list_tasks(&simulation, state - 1);
            } else {
                puts("Durum seçimi geçersiz.");
            }
            break;
        }
        case 4: query_task(&simulation); break;
        case 5: show_technicians(&simulation); break;
        case 6:
            if (!simulation_advance_by(&simulation, 1)) {
                puts("Önce bir algoritma seçin veya zaman sınırını kontrol edin.");
            }
            break;
        case 7: {
            int units;
            if (!read_integer("İlerletilecek pozitif birim sayısı: ", &units)) {
                break;
            }
            if (units <= 0) {
                puts("Birim sayısı pozitif olmalıdır.");
            } else if (!simulation_advance_by(&simulation, units)) {
                puts("Zaman ilerletilemedi; algoritmayı ve girilen değeri kontrol edin.");
            }
            break;
        }
        case 8: {
            int target;
            if (!read_integer("Hedef zaman (0 veya daha büyük): ", &target)) {
                break;
            }
            if (target < 0) {
                puts("Hedef zaman negatif olamaz.");
            } else if (!simulation_advance_to(&simulation, target)) {
                puts("Hedef zamana gidilemedi; önce algoritma seçin.");
            }
            break;
        }
        case 9: show_statistics(&simulation); break;
        case 10:
            if (simulation.algorithm == ALGORITHM_NONE) {
                puts("Sıfırlamadan önce algoritma seçin.");
            } else if (simulation_start(&simulation, simulation.algorithm,
                                        simulation.round_robin_quantum)) {
                puts("Simülasyon zaman 0'dan sıfırlandı.");
            } else {
                puts("Simülasyon sıfırlanamadı.");
            }
            break;
        case 0: running = false; break;
        default: puts("Menü seçimi geçersiz."); break;
        }
    }

    simulation_destroy(&simulation);
    puts("Program sonlandırıldı.");
    return EXIT_SUCCESS;
}

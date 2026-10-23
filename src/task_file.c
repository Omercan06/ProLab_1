#include "prolab.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TASK_LINE_BUFFER_SIZE 512

static bool parse_nonnegative_int(const char *text, int *value)
{
    char *end;
    long parsed;

    if (text == NULL || *text == '\0') {
        return false;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool parse_positive_int(const char *text, int *value)
{
    return parse_nonnegative_int(text, value) && *value > 0;
}

static bool parse_location(const char *text, Location *location)
{
    static const char *const names[LOCATION_COUNT] = {
        "LAB_A", "LAB_B", "DERSLIK_1", "DERSLIK_2", "DERSLIK_3"
    };
    int i;

    for (i = 0; i < LOCATION_COUNT; ++i) {
        if (strcmp(text, names[i]) == 0) {
            *location = (Location)i;
            return true;
        }
    }
    return false;
}

static bool has_id(const Simulation *simulation, int id)
{
    size_t i;

    for (i = 0; i < simulation->task_count; ++i) {
        if (simulation->tasks[i].id == id) {
            return true;
        }
    }
    return false;
}

static bool ensure_task_capacity(Simulation *simulation)
{
    size_t new_capacity;
    Task *new_tasks;

    if (simulation->task_count < simulation->task_capacity) {
        return true;
    }
    if (simulation->task_capacity == 0) {
        new_capacity = PROLAB_DEFAULT_TASK_CAPACITY;
    } else {
        if (simulation->task_capacity > (size_t)-1 / 2 / sizeof(*simulation->tasks)) {
            return false;
        }
        new_capacity = simulation->task_capacity * 2;
    }

    new_tasks = realloc(simulation->tasks, new_capacity * sizeof(*simulation->tasks));
    if (new_tasks == NULL) {
        return false;
    }
    simulation->tasks = new_tasks;
    simulation->task_capacity = new_capacity;
    return true;
}

static void report_bad_line(FILE *stream, size_t line_number, const char *reason)
{
    if (stream != NULL) {
        fprintf(stream, "Satir %zu reddedildi: %s\n", line_number, reason);
    }
}

static bool parse_task_line(char *line, Task *task, const char **reason)
{
    char *fields[7];
    char *token;
    size_t count = 0;
    int priority;

    token = strtok(line, " \t\r\n\v\f");
    while (token != NULL && count < 7) {
        fields[count++] = token;
        token = strtok(NULL, " \t\r\n\v\f");
    }
    if (count != 6) {
        *reason = count > 6 ? "fazla alan (6 alan bekleniyor)"
                            : "eksik alan (6 alan bekleniyor)";
        return false;
    }
    if (!parse_positive_int(fields[0], &task->id)) {
        *reason = "ID pozitif bir tam sayi olmali";
        return false;
    }
    if (!parse_nonnegative_int(fields[1], &task->arrival_time)) {
        *reason = "gelis zamani sifir veya daha buyuk bir tam sayi olmali";
        return false;
    }
    if (!parse_positive_int(fields[2], &task->total_duration)) {
        *reason = "sure pozitif bir tam sayi olmali";
        return false;
    }
    if (!parse_positive_int(fields[3], &priority) || priority > 5) {
        *reason = "oncelik 1 ile 5 arasinda olmali";
        return false;
    }
    if (strlen(fields[4]) > PROLAB_TASK_TYPE_MAX) {
        *reason = "gorev turu 39 karakterden uzun";
        return false;
    }
    if (!parse_location(fields[5], &task->location)) {
        *reason = "gecersiz konum";
        return false;
    }

    task->priority = priority;
    strcpy(task->type, fields[4]);
    task->state = TASK_NOT_ARRIVED;
    task->remaining_duration = task->total_duration;
    task->worked_units = 0;
    task->current_technician = -1;
    task->first_start_time = -1;
    task->completion_time = -1;
    task->last_technician = -1;
    return true;
}

bool task_file_load(const char *path, Simulation *simulation,
                    TaskLoadReport *report, FILE *diagnostics)
{
    FILE *input;
    char line[TASK_LINE_BUFFER_SIZE];
    size_t line_number = 0;
    TaskLoadReport local_report = { 0, 0, 0 };

    if (path == NULL || simulation == NULL || simulation->tasks == NULL) {
        return false;
    }
    input = fopen(path, "r");
    if (input == NULL) {
        if (diagnostics != NULL) {
            fprintf(diagnostics, "Dosya acilamadi: %s\n", path);
        }
        return false;
    }

    simulation->task_count = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        char *first = line;
        size_t length;
        Task parsed;
        const char *reason = NULL;

        ++line_number;
        length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(input)) {
            int ch;
            while ((ch = fgetc(input)) != '\n' && ch != EOF) {
                /* Drain the remainder so the next read starts on a new line. */
            }
            ++local_report.rejected_lines;
            report_bad_line(diagnostics, line_number, "satir cok uzun");
            continue;
        }

        while (*first == ' ' || *first == '\t' || *first == '\r' || *first == '\n' ||
               *first == '\v' || *first == '\f') {
            ++first;
        }
        if (*first == '\0') {
            ++local_report.skipped_lines;
            continue;
        }
        if (*first == '#') {
            ++local_report.skipped_lines;
            continue;
        }

        task_initialize(&parsed);
        if (!parse_task_line(line, &parsed, &reason)) {
            ++local_report.rejected_lines;
            report_bad_line(diagnostics, line_number, reason);
            continue;
        }
        if (has_id(simulation, parsed.id)) {
            ++local_report.rejected_lines;
            report_bad_line(diagnostics, line_number, "yinelenen ID; ilk gecerli kayit korundu");
            continue;
        }
        if (!ensure_task_capacity(simulation)) {
            if (diagnostics != NULL) {
                fprintf(diagnostics, "Bellek yetersiz: gorev kapasitesi artirilamadi.\n");
            }
            fclose(input);
            if (report != NULL) {
                *report = local_report;
            }
            return false;
        }
        simulation->tasks[simulation->task_count++] = parsed;
        ++local_report.accepted_lines;
    }

    if (ferror(input)) {
        if (diagnostics != NULL) {
            fprintf(diagnostics, "Dosya okunurken hata olustu.\n");
        }
        fclose(input);
        if (report != NULL) {
            *report = local_report;
        }
        return false;
    }
    fclose(input);
    if (report != NULL) {
        *report = local_report;
    }
    return true;
}

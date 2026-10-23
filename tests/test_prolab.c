#include "prolab.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_task(Task *task, int id, int arrival, int duration,
                     int priority, Location location)
{
    task_initialize(task);
    task->id = id;
    task->arrival_time = arrival;
    task->total_duration = duration;
    task->remaining_duration = duration;
    task->priority = priority;
    task->location = location;
    strcpy(task->type, "TEST_TASK");
}

static void test_loader_and_capacity(void)
{
    const char *path = "test_tasks_generated.tmp";
    Simulation simulation;
    TaskLoadReport report;
    FILE *file;
    int i;

    assert(simulation_init(&simulation));
    file = fopen(path, "w");
    assert(file != NULL);
    fputs("# test comment\n\n", file);
    for (i = 1; i <= 1002; ++i) {
        fprintf(file, "%d %d 2 3 JOB LAB_A\n", i, i % 4);
    }
    fputs("1 0 9 1 DUP LAB_B\n", file);            /* duplicate */
    fputs("2000 0 0 3 BAD_DURATION LAB_A\n", file); /* invalid duration */
    fputs("2001 0 2 3 TOO FEW\n", file);            /* too few fields */
    fputs("2002 0 2 3 JOB LAB_A EXTRA\n", file);    /* too many fields */
    fputs("2003 0 2 3 JOB UNKNOWN\n", file);        /* invalid location */
    assert(fclose(file) == 0);

    assert(task_file_load(path, &simulation, &report, NULL));
    assert(simulation.task_count == 1002);
    assert(report.accepted_lines == 1002);
    assert(report.rejected_lines == 5);
    assert(report.skipped_lines == 2);
    assert(simulation.tasks[0].id == 1);
    assert(simulation.tasks[0].remaining_duration == 2);
    assert(!task_file_load("missing_test_tasks.tmp", &simulation, &report, NULL));
    assert(simulation.task_count == 1002); /* failed open keeps current records */
    assert(remove(path) == 0);
    simulation_destroy(&simulation);
}

static void test_empty_file_and_priority_ties(void)
{
    const char *path = "test_tasks_empty.tmp";
    Simulation simulation;
    TaskLoadReport report;
    FILE *file;

    assert(simulation_init(&simulation));
    file = fopen(path, "w");
    assert(file != NULL);
    assert(fclose(file) == 0);
    assert(task_file_load(path, &simulation, &report, NULL));
    assert(simulation.task_count == 0 && report.accepted_lines == 0);
    assert(remove(path) == 0);

    simulation.task_count = 3;
    set_task(&simulation.tasks[0], 10, 0, 1, 2, LOCATION_LAB_A);
    set_task(&simulation.tasks[1], 5, 0, 2, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[2], 3, 0, 1, 1, LOCATION_LAB_A);
    assert(simulation_start(&simulation, ALGORITHM_PRIORITY, 0));
    assert(simulation.tasks[2].current_technician == 0);
    assert(simulation.tasks[1].current_technician == 1);
    assert(simulation.tasks[0].current_technician == 2);
    assert(!simulation_start(&simulation, ALGORITHM_ROUND_ROBIN, 0));
    simulation_destroy(&simulation);
}

static void test_time_advance_transition_and_rewind(void)
{
    Simulation stepped;
    Simulation jumped;
    size_t i;

    assert(simulation_init(&stepped));
    assert(simulation_init(&jumped));
    stepped.task_count = jumped.task_count = 3;
    set_task(&stepped.tasks[0], 10, 0, 3, 5, LOCATION_LAB_A);
    set_task(&stepped.tasks[1], 2, 0, 1, 4, LOCATION_LAB_A);
    set_task(&stepped.tasks[2], 7, 1, 1, 1, LOCATION_LAB_B);
    for (i = 0; i < stepped.task_count; ++i) {
        jumped.tasks[i] = stepped.tasks[i];
    }

    assert(simulation_start(&stepped, ALGORITHM_SHORTEST_JOB, 0));
    assert(simulation_start(&jumped, ALGORITHM_SHORTEST_JOB, 0));
    assert(stepped.tasks[1].current_technician == 0);
    assert(stepped.tasks[0].current_technician == 1);
    assert(simulation_advance_to(&jumped, 3));
    assert(simulation_advance_to(&jumped, 3)); /* same target is a no-op */
    assert(simulation_advance_by(&stepped, 1));
    assert(simulation_advance_by(&stepped, 1));
    assert(simulation_advance_by(&stepped, 1));
    assert(stepped.current_time == jumped.current_time);
    assert(stepped.tasks[2].first_start_time == 2); /* one unit of location transition */
    assert(stepped.tasks[2].completion_time == 3);
    for (i = 0; i < stepped.task_count; ++i) {
        assert(stepped.tasks[i].state == jumped.tasks[i].state);
        assert(stepped.tasks[i].remaining_duration == jumped.tasks[i].remaining_duration);
        assert(stepped.tasks[i].completion_time == jumped.tasks[i].completion_time);
    }

    assert(simulation_advance_to(&stepped, 1));
    assert(stepped.current_time == 1);
    assert(stepped.tasks[2].state == TASK_RUNNING);
    assert(stepped.tasks[2].first_start_time == -1);
    simulation_destroy(&stepped);
    simulation_destroy(&jumped);
}

static void test_round_robin_and_custom(void)
{
    Simulation simulation;

    assert(simulation_init(&simulation));
    simulation.task_count = 2;
    set_task(&simulation.tasks[0], 1, 0, 2, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[1], 2, 0, 2, 1, LOCATION_LAB_A);
    assert(simulation_start(&simulation, ALGORITHM_ROUND_ROBIN, 1));
    assert(simulation_advance_to(&simulation, 2));
    assert(simulation.tasks[0].completion_time == 2);
    assert(simulation.tasks[1].completion_time == 2);
    assert(simulation.round_robin_queue_count == 0);

    simulation.task_count = 6;
    set_task(&simulation.tasks[0], 10, 0, 3, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[1], 20, 0, 3, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[2], 30, 0, 3, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[3], 40, 0, 3, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[4], 5, 1, 5, 1, LOCATION_LAB_A);
    set_task(&simulation.tasks[5], 3, 1, 5, 1, LOCATION_LAB_A);
    assert(simulation_start(&simulation, ALGORITHM_ROUND_ROBIN, 1));
    assert(simulation_advance_to(&simulation, 1));
    assert(simulation.tasks[5].current_technician == 0); /* new arrivals precede expiries */
    assert(simulation.tasks[4].current_technician == 1);
    assert(simulation.tasks[0].current_technician == 2);
    assert(simulation.tasks[1].current_technician == 3);

    simulation.task_count = 4;
    set_task(&simulation.tasks[0], 1, 0, 1, 3, LOCATION_LAB_A);
    set_task(&simulation.tasks[1], 2, 0, 10, 3, LOCATION_LAB_B);
    set_task(&simulation.tasks[2], 3, 1, 2, 3, LOCATION_LAB_A);
    set_task(&simulation.tasks[3], 4, 1, 1, 3, LOCATION_LAB_B);
    assert(simulation_start(&simulation, ALGORITHM_CUSTOM, 0));
    assert(simulation_advance_to(&simulation, 1));
    assert(simulation.tasks[2].current_technician == 0);
    assert(simulation.tasks[2].first_start_time == 1);
    simulation_destroy(&simulation);
}

static int run_to_completion(Simulation *simulation, SchedulingAlgorithm algorithm,
                             int quantum)
{
    size_t i;
    bool complete = false;

    assert(simulation_start(simulation, algorithm, quantum));
    while (!complete && simulation->current_time < 100) {
        assert(simulation_advance_by(simulation, 1));
        complete = true;
        for (i = 0; i < simulation->task_count; ++i) {
            if (simulation->tasks[i].state != TASK_COMPLETED) {
                complete = false;
            }
        }
    }
    assert(complete);
    {
        int makespan = 0;
        for (i = 0; i < simulation->task_count; ++i) {
            if (simulation->tasks[i].completion_time > makespan) {
                makespan = simulation->tasks[i].completion_time;
            }
        }
        return makespan;
    }
}

static void test_location_cost_comparison(void)
{
    Simulation simulation;
    TaskLoadReport report;

    assert(simulation_init(&simulation));
    assert(task_file_load("examples/location_cost_scenario.txt", &simulation,
                          &report, NULL));
    assert(report.accepted_lines == 12 && report.rejected_lines == 0);
    assert(run_to_completion(&simulation, ALGORITHM_SHORTEST_JOB, 0) == 21);
    assert(run_to_completion(&simulation, ALGORITHM_PRIORITY, 0) == 20);
    assert(run_to_completion(&simulation, ALGORITHM_ROUND_ROBIN, 2) == 22);
    assert(run_to_completion(&simulation, ALGORITHM_CUSTOM, 0) == 19);
    simulation_destroy(&simulation);
}

int main(void)
{
    test_loader_and_capacity();
    test_empty_file_and_priority_ties();
    test_time_advance_transition_and_rewind();
    test_round_robin_and_custom();
    test_location_cost_comparison();
    puts("All ProLab tests passed.");
    return 0;
}

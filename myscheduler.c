#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

//  CITS2002 Project 1 2023
//  Student1:   23196696   Charles-Aidan-Watson

//  myscheduler (v1.0)
//  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c

//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF sysconfig AND command DETAILS
//  THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE CONSTANTS
//  WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES 4
#define MAX_DEVICE_NAME 20
#define MAX_COMMANDS 10
#define MAX_COMMAND_NAME 20
#define MAX_SYSCALLS_PER_PROCESS 40
#define MAX_RUNNING_PROCESSES 50

//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

#define DEFAULT_TIME_QUANTUM 100

#define TIME_CONTEXT_SWITCH 5
#define TIME_CORE_STATE_TRANSITIONS 10
#define TIME_ACQUIRE_BUS 20

#define CHAR_COMMENT '#'

//  ----------------------------------------------------------------------

// device datastructs + constructors
struct device
{
    char name[MAX_DEVICE_NAME]; // device name
    int read;                   // read speed bps
    int write;                  // write speed bps
};

// devices list of type device
struct device devices[MAX_DEVICES];

int customTimeQuantum = -1;
// any custom time quantum will be positive, so can set to negative while
// processing sysconf and update before executing commands

void newDevice(struct device *dev,
               char *name_dev, int read_bps, int write_bps)
{
    strcpy(dev->name, name_dev);
    dev->read = read_bps;
    dev->write = write_bps;
}

// syscall / instruction and command datastructs + constructors
struct instruction
{
    // true if syscall, false if command from commands list
    // if true, only data in here will be name and elapsed_time
    bool syscall;

    bool uses_devices;

    // these two values are ALWAYS required
    int elapsed_time;            // usecs
    char name[MAX_COMMAND_NAME]; // instruct / syscall / command name

    // if given will be using bytes in out
    // if not given,
    char device_name[MAX_DEVICE_NAME];

    // only one of these will be given
    int processing_time; // usecs
    int bytes_in_out;    // bytes
};

// for command status during runtime simulation
typedef enum
{
    Ready,
    Running,
    Blocked,
    Completed
} Status;

struct command
{
    char name[MAX_COMMAND_NAME];

    // updated by add_syscall
    int num_instructions;
    struct instruction instructions[MAX_SYSCALLS_PER_PROCESS];

    // state settings, always initialised as Ready and 0
    Status state;
    bool needs_Bus;
    int curr_instruction;
};

// initialise commands list
struct command commands[MAX_COMMANDS];
int num_commands = 0;

void add_command(char command_name[MAX_COMMAND_NAME])
{
    strcpy(commands[num_commands].name, command_name);
    commands[num_commands].num_instructions = 0;
    commands[num_commands].curr_instruction = 0;
    commands[num_commands].state = Ready;
    commands[num_commands].needs_Bus = false;
    num_commands++;
}

void add_syscall(struct command *cmd,
                 bool is_syscall, bool device_used, int elapsed_usecs,
                 char cmd_name[MAX_COMMAND_NAME], char dev_name[MAX_DEVICE_NAME],
                 int ps_time, int bytes)
{
    // write all values to the given command *cmd
    cmd->instructions[cmd->num_instructions].syscall = is_syscall;
    cmd->instructions[cmd->num_instructions].uses_devices = device_used;
    strcpy(cmd->instructions[cmd->num_instructions].name, cmd_name);
    cmd->instructions[cmd->num_instructions].elapsed_time = elapsed_usecs;
    strcpy(cmd->instructions[cmd->num_instructions].device_name, dev_name);
    cmd->instructions[cmd->num_instructions].processing_time = ps_time;
    cmd->instructions[cmd->num_instructions].bytes_in_out = bytes;
    // increment num of instructions
    cmd->num_instructions = cmd->num_instructions + 1;
    // WHY DOES ++ NOT INCREMENT?
}

//  ----------------------------------------------------------------------

void read_sysconfig(char argv0[], char filename[])
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }

    // assuming line length of 256
    char line[256];
    int device_count = 0;
    // local time quantum to store actual custom quantum in before modifying global
    int custom_time_quantum = -1;

    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == CHAR_COMMENT)
        {
            // Skip comment lines
            continue;
        }

        // for casting as local variables before sending them to new_device
        char name[MAX_DEVICE_NAME];
        int read_speed, write_speed;

        // +6 is not very nice but need to skip the first 6 characters aka "device"
        // there are a lot of assumptions here, including that the format of device
        // lines will not be replicated, otherwise timequantum 100 100 would make a
        // new device with name 'antum' read=100 write=100, but this is a relatively
        // safe assumption since it was specified that the format will be valid
        if (sscanf(line + 6, "%s %d Bps %d Bps", name, &read_speed, &write_speed) == 3)
        {
            if (device_count < MAX_DEVICES)
            {
                newDevice(&devices[device_count], name, read_speed, write_speed);
                device_count++;
            }
        }
        else if (sscanf(line, "timequantum %d", &custom_time_quantum) == 1)
        {
            // scanning and setting and set custom time quantum entry
            customTimeQuantum = custom_time_quantum;
        }
    }

    // close the file
    fclose(file);
}

//  ----------------------------------------------------------------------

bool usecsOccursTwice(char *ln)
{

    // strstr checks if a string is in a string
    char *firstOccurrence = strstr(ln, "usecs");

    // if there is 1 usecs, we check again from the point after
    // the first usecs ie: "usecs10 usecs" sees "usecs" at 0, and then
    // checks again within the string "secs10 usecs", finding the second one
    if (firstOccurrence != NULL)
    {
        char *secondOccurrence = strstr(firstOccurrence + 1, "usecs");
        return secondOccurrence != NULL;
    }

    return false;
}

void read_commands(char argv[0], const char *filename)
{

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        puts("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[256]; // assuming line length of 256
    int command_counter = num_commands - 1;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (line[0] == CHAR_COMMENT)
        {
            // skip comment lines
            continue;
        }

        // unintuitive variable names but makes sense when the columns
        // are only distinguishable via the number of said columns
        int elapsed_time;
        char word1[MAX_COMMAND_NAME];
        char word2[MAX_COMMAND_NAME];
        char word3[MAX_DEVICE_NAME];
        int ps_time_or_byte_size;
        char word4[MAX_COMMAND_NAME];

        if (usecsOccursTwice(line))
        {
            sscanf(line, "%d %s %s %d %s",
                   &elapsed_time, word1, word2, &ps_time_or_byte_size, word3);
            add_syscall(&commands[command_counter],
                        true, false, elapsed_time, word2, "", ps_time_or_byte_size, 0);
            continue;
        }
        int sscanf_result = sscanf(line, "%d %s %s %s %d %s",
                                   &elapsed_time, word1, word2, word3, &ps_time_or_byte_size, word4);

        // printf("Line: %s\n", line);
        // printf("sscanf_result: %d\n", sscanf_result);

        switch (sscanf_result)
        {
        case 0:
            // no string detected since first word in line is a num
            sscanf(line, "%s", word1);
            add_command(word1);
            command_counter++;
            break;
            // note there is no case of 2 ever
        case 3:
            // Syscall of type wait or exit
            add_syscall(&commands[command_counter],
                        true, false, elapsed_time, word2,
                        "", 0, 0);
            break;
        case 4:
            // spawn
            add_syscall(&commands[command_counter],
                        true, false, elapsed_time, word3,
                        "", 0, 0);

            // printf("word1:%s elapsed:%d word2:%s word3:%s int detected:%d word4:%s", word1, elapsed_time, word2, word3, ps_time_or_byte_size, word4);
            break;
        case 5:
            // syscall is used with duration specified
            add_syscall(&commands[command_counter],
                        true, false, elapsed_time, word2,
                        "", ps_time_or_byte_size, 0);
            break;
        case 6:
            // device used
            add_syscall(&commands[command_counter],
                        true, true, elapsed_time,
                        word2, word3, 0, ps_time_or_byte_size);

            // set is_syscall = true (command name = read or write in word 2)
            // device name was scanned into `word3`
            // 0 ps time, will be calculated from the byte_size instead
            // using the read and write speed of the device from devices[]
            break;
        default:
            fprintf(stderr, "Error parsing line: %s", line);
            exit(EXIT_FAILURE);
        }
    }

    // Close the file
    fclose(file);
}

//  ----------------------------------------------------------------------

void print_sysconfig()
{
    // Printing out the data read by read_sysconfig
    for (int i = 0; i < MAX_DEVICES; i++)
    {
        if (devices[i].name[0] != '\0')
        {
            printf("Device %d: Name=%s, Read=%d, Write=%d\n",
                   i, devices[i].name, devices[i].read, devices[i].write);
        }
    }
    if (customTimeQuantum != -1)
    {
        printf("Custom time quantum: %d\n", customTimeQuantum);
    }
    else
    {
        printf("Using default time quantum: %d\n", DEFAULT_TIME_QUANTUM);
        customTimeQuantum = DEFAULT_TIME_QUANTUM;
    }
    puts("\n");
}

void print_commands()
{
    for (int cmd_index = 0; cmd_index < num_commands; cmd_index++)
    {
        printf("Command %s has %d instructions\n",
               commands[cmd_index].name, commands[cmd_index].num_instructions);

        for (int syscall_idx = 0;
             syscall_idx < commands[cmd_index].num_instructions;
             syscall_idx++)
        {
            printf("\t syscall=%d device_used=%d elapsed_time=%d ",
                   commands[cmd_index].instructions[syscall_idx].syscall,
                   commands[cmd_index].instructions[syscall_idx].uses_devices,
                   commands[cmd_index].instructions[syscall_idx].elapsed_time);
            printf("instruction_name=%s device_name=%s processing_time=%d bytes_processed=%d \n",
                   commands[cmd_index].instructions[syscall_idx].name,
                   commands[cmd_index].instructions[syscall_idx].device_name,
                   commands[cmd_index].instructions[syscall_idx].processing_time,
                   commands[cmd_index].instructions[syscall_idx].bytes_in_out);
        }
        puts("\n");
    }
}

//  ----------------------------------------------------------------------
// transitions the given commands state to whatever `status` is specified
void transition(int *sys_time, struct command *cmd, Status status)
{
    switch (status)
    {
    case Ready:
        (*sys_time) += 10;
        cmd->state = Ready;
        puts("process moved to ready\n");
        break;
    case Running:
        (*sys_time) += 5;
        cmd->state = Running;
        puts("process moved to running\n");
        break;
    case Blocked:
        (*sys_time) += 10;
        cmd->state = Blocked;
        puts("process moved to blocked\n");
        break;
    case Completed:
        cmd->state = Completed;
        printf("%s execution complete\n\n", cmd->name);
        break;
    default:
        puts("bad state transition\n");
        exit(EXIT_FAILURE);
    }
}

void sysc_read_write(int *sys_time, struct command *cmd, bool *has_bus)
{
    int usecs_spent_xfering = 0;
    // true if reading, false if writing
    bool reading = (strcmp(cmd->instructions[cmd->curr_instruction].name, "read") == 0) ? true : false;

    // get read / write speed and data payload size
    int speed = 0;
    int data_size = cmd->instructions[cmd->curr_instruction].bytes_in_out;

    for (int i = 0; i < MAX_DEVICES; i++)
    {
        if (strcmp(cmd->instructions[cmd->curr_instruction].device_name, devices[i].name) == 0)
        {
            printf("Matched device: %s\n", devices[i].name);
            speed = (reading) ? devices[i].read : devices[i].write;
            break;
        }
    }
    printf("%d bytes/s device speed detected\n", speed);

    // workaround for conversions between b/sec and b/usec

    int64_t temp_speed = (int64_t)speed;
    // data size needs to be 1M* bigger to interop with usecs properly
    int64_t temp_data_size = (int64_t)data_size * 1000 * 1000;
    int64_t temp_xfer_usecs = temp_data_size / temp_speed;

    // cast back down to default int
    usecs_spent_xfering = (int)temp_xfer_usecs;
    printf("Spent %d usecs transferring %d bytes at %d bytes/s\n\n",
           usecs_spent_xfering, data_size, speed);

    // increment the commands instruction id as read / write is now complete
    cmd->curr_instruction = cmd->curr_instruction + 1;
    *sys_time += usecs_spent_xfering;
}

void sysc_exit(int *sys_time, struct command *cmd)
{
    puts("exiting");
    // we decrement as the current usec is spent exiting, which takes 0 usecs
    *sys_time -= 1;
    transition(sys_time, cmd, Completed);
}

void sysc_sleep(int *sys_time, struct command *cmd)
{
    // sleep system for specified sleep time
    *sys_time += cmd->instructions[cmd->curr_instruction].processing_time;
    // increment the commands instruction id as sleep is completed
    cmd->curr_instruction = cmd->curr_instruction + 1;
}

// start at 0 because process bus id is initialised as 0
int bus_process_id = 1;

int total_usecs = 0;
int cpu_util = 0;

void process(struct command *cmd)
{
    // data bus state commands
    bool process_has_bus = false;
    printf("PROCESSING COMMAND %s.\n\n", cmd->name);

    int system_time = 0;
    int onCpu = 0;

    while (cmd->state != Completed)
    {
        system_time++;
        // if the bus_process_id is not the local bus id, the process no longer has the bus

        switch (cmd->state)
        {
        case Ready:
            transition(&system_time, cmd, Running);
            break;

        case Running:
            if (onCpu != cmd->instructions[cmd->curr_instruction].elapsed_time)
            {
                // execute on a new time quantum
                int prev_onCpu = onCpu; // temporary counter for this scopes logic

                puts("new time quantum");
                printf("length of cumulative exec time (aka quantum start time): %d\n", onCpu);
                printf("length of total processing time, including quantum time: %d\n\n", system_time);

                // while current quantum time less than the specified quantum length, execute
                for (int quantum = prev_onCpu; quantum < (prev_onCpu + customTimeQuantum); quantum++)
                {
                    if (quantum < cmd->instructions[cmd->curr_instruction].elapsed_time)
                    {
                        onCpu++;
                        system_time++;
                    }
                    else if (onCpu == cmd->instructions[cmd->curr_instruction].elapsed_time)
                    {
                        // if execution completed within the current quantum
                        printf("execution completed at %d usecs, transitioning command\n", system_time);
                        break;
                    }
                }
            }
            // check if execution is complete so can then run syscall
            if (onCpu == cmd->instructions[cmd->curr_instruction].elapsed_time)
            {
                printf("execution on cpu done, running syscall / command: %s\n", cmd->instructions[cmd->curr_instruction].name);
                // printf("TOTAL EXEC TIME AT SYSCALL START:%d\n", onCpu);
                printf("oncpu total time = %d\nsystem total time = %d\n", onCpu, system_time);

                // if the instruction is a read / write using a device:
                if (cmd->instructions[cmd->curr_instruction].uses_devices)
                {
                    // if the prev instruction was not the same type as the current AND ps doesnt have bus
                    if (process_has_bus == false)
                    {
                        transition(&system_time, cmd, Blocked);
                        break;
                    }
                    else
                    {
                        // else we do have the bus and are not blocked
                        sysc_read_write(&system_time, cmd, &process_has_bus);
                        transition(&system_time, cmd, Ready);
                        process_has_bus = false;
                    }
                    break;
                    // if exiting
                }
                else if (strcmp(cmd->instructions[cmd->curr_instruction].name, "exit") == 0)
                {
                    sysc_exit(&system_time, cmd);
                }
                else if (strcmp(cmd->instructions[cmd->curr_instruction].name, "sleep") == 0)
                {
                    sysc_sleep(&system_time, cmd);
                    transition(&system_time, cmd, Ready);
                }
                else if (strcmp(cmd->instructions[cmd->curr_instruction].name, "wait") == 0)
                {
                    cmd->curr_instruction = cmd->curr_instruction + 1;
                    transition(&system_time, cmd, Blocked);
                }
                else
                {
                    // any syscall not matched in the previous R/W/E/S/W commands is always spawn
                    printf("Spawning new process: %s\n", cmd->instructions[cmd->curr_instruction].name);
                    struct command spawned_command;
                    for (int i = 0; i < MAX_COMMANDS; i++)
                    {
                        if (strcmp(commands[i].name, cmd->instructions[cmd->curr_instruction].name) == 0)
                        {
                            spawned_command = commands[i];
                            break;
                        }
                        else if (i == MAX_COMMANDS)
                        {
                            puts("Command to spawn not found");
                            exit(EXIT_FAILURE);
                        }
                    }
                    process(&spawned_command);
                    cmd->curr_instruction = cmd->curr_instruction + 1;
                    transition(&system_time, cmd, Ready);
                }
                break;
            }
            else
            {
                transition(&system_time, cmd, Ready);
            }
            break;
        case Blocked:
            // if blocked due to needing data bus and the process does not have it:
            if (cmd->instructions[cmd->curr_instruction].uses_devices && process_has_bus == false)
            {
                puts("getting data bus for next 20 usecs");
                system_time += 20;
                process_has_bus = true;
            }
            // then transition
            transition(&system_time, cmd, Running);
            break;
        default:
            puts("bad command\n");
            exit(EXIT_FAILURE);
        }
    }
    cpu_util += onCpu;
    total_usecs += system_time;
}

void execute_commands()
{
    puts("----------------------------------------------------------------------");
    puts("Begin executing commands.");
    process(&commands[0]);
    printf("total time=%d cpu_util_time=%d \n", total_usecs, cpu_util);
    puts("----------------------------------------------------------------------");
}

//  ----------------------------------------------------------------------

// returns the percentage of exec time as an int
double calculateFinalUtil(int num, int total)
{
    if (total == 0)
    {
        // avoid div by 0
        return 0;
    }
    double percentage = ((double)num / total) * 100.0;
    return (int)floor(percentage);
}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    //  ENSURE THAT WE HAVE THE CORRECT NUMBER OF COMMAND-LINE ARGUMENTS
    if (argc != 3)
    {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //  READ THE COMMAND FILE FIRST
    read_commands(argv[0], argv[2]);
    print_commands();

    //  READ THE SYSTEM CONFIGURATION FILE
    read_sysconfig(argv[0], argv[1]);
    print_sysconfig();

    //  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
    execute_commands();

    //  PRINT THE PROGRAM'S RESULTS
    int final_util_out = calculateFinalUtil(cpu_util, total_usecs);
    printf("measurements  %i  %i\n", total_usecs, final_util_out);

    exit(EXIT_SUCCESS);
}

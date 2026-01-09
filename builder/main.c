#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;

#define MAX_ARGUMENTS 3

typedef struct
{

    char *arg_type[MAX_ARGUMENTS];

} BuildConfig;

typedef enum
{
    KERNEL_BUILD,
    BUILD_INSTALLER,
    START_TEST, // after
    GENERATE_ISO,
    EXIT
} OptionSelect;

static void
__init_build(BuildConfig)
{

    BuildConfig *build_cfg = malloc(sizeof(BuildConfig));
    if (!build_cfg)
    {
        perror("error init build configuration\n");

        free_resource(build_cfg);
    }
}

void free_resource(char *buffer, ...)
{

    free(buffer);
}

// ui
void welcome_window()
{
    printf("Hello, welcome to Lain Builder v0.1\n");
    printf("You can select another options(check down up): \n");
    printf("List: \n");
    printf("0. Kernel Build (default linux-hearded)\n");
    printf("1. Build Installer (Lainux Installer TUI)\n");
    printf("2. Start TEST(no release)\n");
    printf("3. Generate Iso Images\n");
    printf("4. EXIT\n");
}

// kernel
void __kernel_configuratio(FILE *pointer, char *filename)
{
    pointer = fopen(filename, "wb");
    if (!pointer)
    {
        printf("'%s' failed to open\n", filename);
    }
}

int main(void)
{
    BuildConfig build_cfg;
    __init_build(build_cfg);

    welcome_window();

    int user_choice;
    int STATUS_EXIT = 1;

    while (STATUS_EXIT)
    {
        printf(">>> Select: ");
        if (scanf("%d", &user_choice) != 1)
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF)
                ;
            printf("[ERROR]: Invalid input. Please enter a number.\n");
            continue;
        }

        switch (user_choice)
        {
        case KERNEL_BUILD:
            printf("Starting kernel build...\n");
            // callback function here
            break;
        case BUILD_INSTALLER:
            printf("Building installer...\n");
            break;
        case START_TEST:
            printf("Running tests...\n");
            break;
        case GENERATE_ISO:
            printf("Generating ISO...\n");
            break;
        case EXIT:
            printf("Exit...\n");
            for (int i = 0; i < 3; ++i)
            {
                printf("** %d\n", i + 1);
                sleep(1);
            }
            STATUS_EXIT = 0;
            break;
        default:
            printf("[ERROR]: Invalid option\n");
            break;
        }
    }

    return 0;
}

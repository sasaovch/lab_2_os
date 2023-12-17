#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#define BUF_SIZE 4097
#define PROC_FILE_PATH "/proc/pci_info_v5"
const char *pci_ids_filepath = "pci.ids";
const char *separator = "\n";
static int is_binary = 0;

bool lookup_ids(const char *ids_filepath, int vendor_bin, int device_bin, char *class_str, char *vendor_str, char *device_str) {
    FILE *file = fopen(ids_filepath, "r");
    if (!file) 
    {
        perror("Error opening pci.ids file");
        return false;
    }

    bool found_vendor = false;
    bool found_device = false;
    bool in_scope = false;

    char line[256];
    while (fgets(line, sizeof(line), file)) 
    {
        unsigned int pci_id, sub_id, sub_id2;
        char sub_name[129];
    
        if (in_scope && sscanf(line, "\t%04X  %128[^\n]", &pci_id, sub_name) == 2) 
        {
            if (device_bin == pci_id) 
            {
                strcpy(device_str, sub_name);
                found_device = true;
                break;
            }
        } else if (sscanf(line, "%04X  %128[^\n]", &pci_id, sub_name) == 2) 
        {
            in_scope = (vendor_bin == pci_id);
            if (in_scope) 
            {
                strcpy(vendor_str, sub_name);
                found_vendor = true;
            }
        }
    }

    fclose(file);
    return (found_vendor && found_device);
}

void print_pci_devices() 
{
    int fd;
    ssize_t read_size;
    char buffer[BUF_SIZE];

    fd = open(PROC_FILE_PATH, O_RDONLY);

    if (fd == -1) 
    {
        perror("Failed to open the proc file");
        return;
    }

    while ((read_size = read(fd, buffer, sizeof(buffer)-1)) > 0) 
    {
        buffer[read_size] = '\0';

        int class_bin = -1, vendor_bin = -1, device_bin = -1;
        char *line = strtok(buffer, separator);
        int found_device = 0;

        while (line != NULL) 
        {
            sscanf(line, "Class: %04X", &class_bin);
            sscanf(line, "Vendor: %04X", &vendor_bin);
            if (sscanf(line, "Device ID: %04X", &device_bin) == 1) 
            {
                char class_str[129];
                char vendor_str[129];
                char device_str[129];
                if (lookup_ids(pci_ids_filepath, vendor_bin, device_bin, class_str, vendor_str, device_str)) 
                {
                    unsigned short int swapped;
                    swapped = ((class_bin & 0xF0F0) >> 4) | ((class_bin & 0x0F0F) << 4);

                    printf("Class: %04X\n", swapped);
                    printf("Vendor: ID - %04X, name - %s\n", vendor_bin, vendor_str);
                    printf("Device: ID - %04X, name - %s\n\n", device_bin, device_str);
                }
                found_device = 1;
            }

            line = strtok(NULL, separator);
        }
        if (!found_device) 
        {
            printf("%s", buffer);
        }
    }

    if (read_size == -1) 
    {
        perror("Failed to read from the proc file");
    }

    close(fd);
}


void send_ids(int vendor_id, int device_id) 
{
    int fd;

    fd = open(PROC_FILE_PATH, O_WRONLY);

    if (fd == -1) 
    {
        perror("Failed to open the proc file for writing");
        return;
    }

    char buffer[64];
    int len = sprintf(buffer, "%x:%x", vendor_id, device_id);

    if (write(fd, buffer, len) == -1) 
    {
        perror("Failed to write the IDs to the proc file");
    }

    close(fd);
}

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTION]\n", prog_name);
    puts("\nOptions:");
    puts("  -a, --all        List all PCI devices.");
    puts("  -h, --help       Display this help message and exit.");
    puts("\nWithout any options, the program prompts for vendor, and device IDs to filter PCI devices.");
}

int main(int argc, char *argv[])
{
    int vendor_id = -1, device_id = -1;
    int option;

    static struct option long_options[] = {
        {"all", no_argument, 0, 'a'},   
        {"help", no_argument, 0, 'h'}, 
        {0, 0, 0, 0}
    };

    int long_index = 0;
    while ((option = getopt_long(argc, argv, "ah", long_options, &long_index)) != -1)
    {
        switch (option)
        {
        case 'a':
            send_ids(vendor_id, device_id);
            printf("Displaying PCI devices\n\n");
            print_pci_devices();
            return 0;
        case 'h':
            print_help(argv[0]);
            return 0;
        default:
            fprintf(stderr, "Usage: %s [-a | -h] or enter IDs for class, vendor and device\n", argv[0]);
            return 1;
        }
    }

    printf("Enter Vendor ID or -1 to skip: ");
    if (scanf("%x", &vendor_id) != 1)
    {
        fprintf(stderr, "Error: Invalid input for Vendor ID\n");
        return 1;
    }
    printf("Enter Device ID or -1 to skip: ");
    if (scanf("%x", &device_id) != 1)
    {
        fprintf(stderr, "Error: Invalid input for Device ID\n");
        return 1;
    }

    send_ids(vendor_id, device_id);
    printf("Displaying filtered PCI devices by vendor %04X, and device %04X:\n\n", vendor_id, device_id);
    print_pci_devices();

    return 0;
}

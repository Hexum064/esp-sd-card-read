/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// This example uses SPI peripheral to communicate with SD card.

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>

static const char *TAG = "example";

#define MOUNT_POINT "/sdcard"

//#define DEBUG
// Pin mapping


#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15


#define SPI_DMA_CHAN    1

// Options for mounting the filesystem.
// If format_if_mount_failed is set to true, SD card will be partitioned and
// formatted in case when mounting fails.
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();

DIR *dr = NULL;
struct dirent *de;  // Pointer for directory entry
long int cur_pos = 0;
long int target_pos = 0;

char path[1024];


void revert_path()
{
    int i = 0;
    int lastPos = -1;

    for (; i<1024 || path[i] == NULL; i++)
    {
        //the next i will always be greater than whatever is stored in lastPos
        if (path[i] == '/')
        {
            lastPos = i;
        }            
    }

    if (lastPos < 0 || path[lastPos] == NULL)
        return;

    path[lastPos] = NULL;

}


void list_files(void)
{
    
  
    // opendir() returns a pointer of DIR type. 
    //DIR *dr = opendir(MOUNT_POINT);
    printf("Printing all files in current directory of %s\n", path);
    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        printf("Could not open current directory" );
        return 0;
    }
  
    // Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html
    // for readdir()
        while ((de = readdir(dr)) != NULL)
            printf("type: %d, name: %s: %d\n", de->d_type, de->d_name, de->d_ino);
    rewinddir(dr);
    printf("Done\n\n");
    //closedir(dr);    
    return 0;
}



// FRESULT scan_files (
//     char* path        /* Start node to be scanned (***also used as work area***) */
// )
// {
//     FRESULT res;
//     FF_DIR dir;
//     UINT i;
//     static FILINFO fno;


//     res = f_opendir(&dir, path);                       /* Open the directory */
//     if (res == FR_OK) {
//         for (;;) {
//             res = f_readdir(&dir, &fno);                   /* Read a directory item */
//             if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
//             if (fno.fattrib & AM_DIR) {                    /* It is a directory */
//                 i = strlen(path);
//                 sprintf(&path[i], "/%s", fno.fname);
//                 res = scan_files(path);                    /* Enter the directory */
//                 if (res != FR_OK) break;
//                 path[i] = 0;
//             } else {                                       /* It is a file. */
//                 printf("%s/%s\n", path, fno.fname);
//             }
//         }
//         f_closedir(&dir);
//     }

//     return res;
// }

void mount_card(void)
{

    esp_err_t ret;


    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");


    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void print_current_file(void)
{

    if (de != NULL)
        printf("%s/%s\n", path, de->d_name);
    else
        printf("File not found\n");        
}

void open_dir(char* name)
{
    
    if (dr != NULL)
        closedir(dr);    

    dr = opendir(name);
}

// void move_forward() 
// {

//     print_current_file();

// }

// void move_back()
// {
//     pos = telldir(dr);
//     if (pos == 1)
//     {
//         printf("No more files\n");
//     }
//     else
//     {
//         pos-=2;
//         seekdir(dr, pos);
//         print_current_file();    
//     }
// }

void navigate_to_target_pos_from_curr_dir() 
{
    long int cur_dir = 0;

    if (target_pos < 1)
        return;

    //first look through the files then recurse through the dirs
    while ((de = readdir(dr)) != NULL)
    {
        if (de->d_type == DT_REG)
        {
            cur_pos++;
#ifdef DEBUG
            printf("Looing at file %s in %s. Pos: %ld, Target: %ld\n", de->d_name, path, cur_pos, target_pos);
#endif        
            if (cur_pos == target_pos)
            {
#ifdef DEBUG                
                printf("File found at pos: %ld\n", cur_pos);
#endif        
                return;
            }
            
#ifdef DEBUG 
            printf("Inc cur_pos to %ld\n", cur_pos);
#endif
        }
    }

    rewinddir(dr);

    //DIRs now
    while ((de = readdir(dr)) != NULL)
    {
        if (de->d_type == DT_DIR)
        {
            cur_dir = telldir(dr);

            strcat(path, "/");
            strcat(path, de->d_name);
            
            open_dir(path);     
#ifdef DEBUG            
            printf("Navigating to path %s\n", path);
#endif            
            navigate_to_target_pos_from_curr_dir();

            if (cur_pos == target_pos)
            {
#ifdef DEBUG                
                printf("File found at pos: %ld\n", cur_pos);
#endif                
                return;
            }

            revert_path();
#ifdef DEBUG            
            printf("Returning to %s\n", path);
#endif            
            open_dir(path);   
            seekdir(dr, cur_dir + 1);
        }
    }  
#ifdef DEBUG
    printf("No more files or dirs in %s\n", path);
#endif    
}

void navigate_to_pos() 
{
    de = NULL;
    strcpy(path, MOUNT_POINT);
    open_dir(path); 
    cur_pos = 0;
    navigate_to_target_pos_from_curr_dir();
}


void get_next_file()
{
    target_pos++;
    navigate_to_pos();
}


void get_prev_file()
{
    if (target_pos > 0)
    {
        target_pos--;
        navigate_to_pos();
    }
}

void app_main(void)
{
    

    strcat (path, MOUNT_POINT);
    

    printf("Path: %s\n", path);

    const char * addPath = "/newPath";
    //char * newPath = path;
    strcat (path, addPath);
    
    //path = newPath;
    printf("Path: %s\n", path);

    revert_path();
    printf("Path: %s\n", path);

    strcat (path, "/nextPath");
    printf("Path: %s\n", path);

    //return;

    // // Use POSIX and C standard library functions to work with files.

    // // First create a file.
    // const char *file_hello = MOUNT_POINT"/hello.txt";

    // ESP_LOGI(TAG, "Opening file %s", file_hello);
    // FILE *f = fopen(file_hello, "w");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for writing");
    //     return;
    // }
    // fprintf(f, "Hello %s!\n", card->cid.name);
    // fclose(f);
    // ESP_LOGI(TAG, "File written");

    // const char *file_foo = MOUNT_POINT"/foo.txt";

    // // Check if destination file exists before renaming
    // struct stat st;
    // if (stat(file_foo, &st) == 0) {
    //     // Delete it if it exists
    //     unlink(file_foo);
    // }

    // // Rename original file
    // ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    // if (rename(file_hello, file_foo) != 0) {
    //     ESP_LOGE(TAG, "Rename failed");
    //     return;
    // }

    // // Open renamed file for reading
    // ESP_LOGI(TAG, "Reading file %s", file_foo);
    // f = fopen(file_foo, "r");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for reading");
    //     return;
    // }

    // // Read a line from file
    // char line[64];
    // fgets(line, sizeof(line), f);
    // fclose(f);

    // // Strip newline
    // char *pos = strchr(line, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);

    mount_card();
    open_dir(MOUNT_POINT);


    list_files();
    target_pos = 0;
    navigate_to_pos();
    print_current_file();

    get_next_file();
    print_current_file();

get_next_file();
    print_current_file();

    get_next_file();
    print_current_file();

    get_next_file();
    print_current_file();

    get_next_file();
    print_current_file();

    get_next_file();
    print_current_file();

    get_prev_file();
    print_current_file();

        get_prev_file();
    print_current_file();

        get_prev_file();
    print_current_file();

        get_prev_file();
    print_current_file();

        get_prev_file();
    print_current_file();

        get_prev_file();
    print_current_file();

    // target_pos = 0;
    // navigate_to_pos();
    // print_current_file();

    // target_pos = 1;
    // navigate_to_pos();
    // print_current_file();
    

    // target_pos = 3;
    // navigate_to_pos();
    // print_current_file();
    
    // target_pos = 2;
    // navigate_to_pos();
    // print_current_file();
    
    // target_pos = 6;
    // navigate_to_pos();
    // print_current_file();    

    // target_pos = 7;
    // navigate_to_pos();
    // print_current_file();  

// print_current_file();
// move_forward() ;
// move_forward() ;
// move_back();
// move_back();
// move_back();
// move_forward() ;
// move_forward() ;
// move_forward() ;
// move_forward(); 
// move_forward(); 
// move_forward(); 
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}

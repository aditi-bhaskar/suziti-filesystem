// TODO 
// create file
// add to file (append)
// remove files


#include "rpi.h" // automatically includes gpio, fat32 stuff
#include "display.c"
#include "pi-sd.h"
#include "fat32.h"

// aditi wire colors: 6: purple, 7: blue, 8: green, 9: yellow, 10: orange
// suze wire colors: TODO

#define BUTTON_SINGLE   's'
#define BUTTON_TOP      't'
#define BUTTON_RIGHT    'r'
#define BUTTON_LEFT     'l'
#define BUTTON_BOTTOM   'b'
#define BUTTON_NONE     'n'

#define input_single 6  
#define input_right 7
#define input_bottom 8
#define input_top 9
#define input_left 10


char unique_file_id = 65; // starts at: "A" // used when creating files


void test_buttons(void){
    while (1) {
        // read gpio
        char which_button = BUTTON_NONE;
        // read the input gpios; ! to measure whether it was pressed down
        if (!gpio_read(input_top)) which_button = BUTTON_TOP;
        if (!gpio_read(input_bottom)) which_button = BUTTON_BOTTOM;
        if (!gpio_read(input_right)) which_button = BUTTON_RIGHT;
        if (!gpio_read(input_left)) which_button = BUTTON_LEFT;
        if (!gpio_read(input_single)) which_button = BUTTON_SINGLE;

        char str[] = {which_button, '\0'};
        display_clear();
        display_write(10, 20, "button pressed:", WHITE, BLACK, 1);
        display_write(10, 45, str, WHITE, BLACK, 2);
        display_update();
    }

}

static uint32_t min(uint32_t a, uint32_t b) {
    if (a < b) {
      return a;
    }
    return b;
}

//******************************************
// FUNCTION HEADERS!!
//******************************************

void show_files(fat32_fs_t *fs, pi_dirent_t *directory) ;

//******************************************
// FUNCTIONS!!
//******************************************



void ls(fat32_fs_t *fs, pi_dirent_t *directory) {
    pi_directory_t files = fat32_readdir(fs, directory);
    printk("Got %d files.\n", files.ndirents);

    for (int i = 0; i < files.ndirents; i++) {
      pi_dirent_t *dirent = &files.dirents[i];
      if (dirent->is_dir_p) {
        printk("\tD: %s (cluster %d)\n", dirent->name, dirent->cluster_id);
      } else {
        printk("\tF: %s (cluster %d; %d bytes)\n", dirent->name, dirent->cluster_id, dirent->nbytes);
      }
    }
}


void create_file(fat32_fs_t *fs, pi_dirent_t *directory) {

    // create a file; name it a random number like demo_{num}
    char filename[10] = {'D','E','M','O',unique_file_id,'.','T','X','T','\0'};
    unique_file_id++;

    ls(fs, directory);

    pi_dirent_t *created_file = fat32_create(fs, directory, filename, 0); // 0=not a directory

    while(1) {
        printk("in create_file, waiting\n");
        display_clear();
        // Display navigation hints
        display_write(0, SSD1306_HEIGHT - 16, "^v<> : write to file", WHITE, BLACK, 1);
        display_write(0, SSD1306_HEIGHT - 8, "* : to exit", WHITE, BLACK, 1);
        display_update();

        // exit file writing
        if (!gpio_read(input_single)) {
            // exit file creation return
            printk("broke out of create_file\n");
            break;
        }

        // these buttons write to file
        if (!gpio_read(input_left)) {
            printk("Reading file\n");
            // pi_file_t *file = fat32_read(fs, directory, filename);

            printk("writing to display\n");

            display_clear();
            display_write(10,10,"writing file!", WHITE, BLACK, 1);
            display_update();

            // write : "prefetch flush* " to the file
            char *data = "*prefetch flush*\n";
            pi_file_t new_file_contents = (pi_file_t) {
              .data = data, // should technically be appending to the old file's txt. do this next
              .n_data = strlen(data),
              .n_alloc = strlen(data),
            };

            printk("writing to fat\n");

            int writ = fat32_write(fs, directory, filename, &new_file_contents);
            printk("wrote *prefetch flush* to file: %s\n", filename);
            // TODO display this stuff on screen

            // DO LS
            ls(fs, directory);

            delay_ms(2000);
            // break;

        }
        if (!gpio_read(input_bottom)) {
            // write : "MORE PIZZA "
            delay_ms(400);
        }
        if (!gpio_read(input_top)) {
            // write : "Dawson "
            delay_ms(400);
        }
        if (!gpio_read(input_right)) {
            // write : "minor "
            delay_ms(400);
        }
        delay_ms(200);

    }
}



void show_menu(fat32_fs_t *fs, pi_dirent_t *directory) {

    printk(" in show menu! \n\n");
    delay_ms(400);

    while(1) {

        display_clear();
        // write (c) on top right for create
        display_draw_char(128 - (6 * 3), 10, '(', WHITE, BLACK, 1);
        display_draw_char(128 - (6 * 2), 10, 'c', WHITE, BLACK, 1);
        display_draw_char(128 - (6 * 1), 10, ')', WHITE, BLACK, 1);

        display_write(10, 2, "menu\n>: select, \n*: exit", WHITE, BLACK, 1);
        display_update();


        if (!gpio_read(input_right)) {
            // call create-file
            create_file(fs, directory);
            delay_ms(200);
            return;
        }
        if (!gpio_read(input_single)) {
            // close menu
            display_clear();
            display_update();
            show_files(fs, directory);
        }

        // TODO implm up/down arrows to select other menu options; highlight them
    }

}

void display_file(fat32_fs_t *fs, pi_dirent_t *directory, pi_dirent_t *file_dirent) {
    trace("about to display file %s\n", file_dirent->name);
    if (!file_dirent || file_dirent->is_dir_p) {
        // Not a valid file
        display_clear();
        display_write(10, 20, "Not a valid file", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
        return;
    }

    // Read the file
    trace("attempt to read file\n", file_dirent->name);
    pi_file_t *file = fat32_read(fs, directory, file_dirent->name);
    if (!file) {
        display_clear();
        display_write(10, 20, "Error reading file", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
        return;
    }
    trace("finished reading file\n", file_dirent->name);

    // Variables for scrolling through file
    int start_line = 0;
    int lines_per_screen = 10;      // Show 10 lines on screen
    int max_lines = file->n_data;   // Maximum number of lines to scroll through

    // Buffer to hold the portion of text to display
    char display_buffer[512]; // Large enough for the visible portion

    while(1) {
        // Format text for display
        display_buffer[0] = '\0';
        int buf_pos = 0;
        int line_count = 0;
        
        // Calculate the actual position in the file to start from
        int file_pos = 0;
        int current_line = 0;
        
        // Skip to starting line
        while (current_line < start_line && file_pos < file->n_data) {
            if (file->data[file_pos] == '\n') {
                current_line++;
            }
            file_pos++;
        }
        
        // Now read lines from the calculated position
        for (int i = file_pos; i < file->n_data && line_count < lines_per_screen; i++) {
            char c = file->data[i];
            
            // Add character to buffer
            if (c == '\r') continue; // Skip carriage returns
            
            display_buffer[buf_pos++] = c;
            
            // Ensure null termination
            display_buffer[buf_pos] = '\0';
            
            // Count lines
            if (c == '\n') {
                line_count++;
            }
            
            // Check if we've completed all lines
            if (line_count >= lines_per_screen) break;
        }

        // Display the text
        display_clear();
        
        // Show file name at the top
        display_write(10, 0, file_dirent->name, WHITE, BLACK, 1);
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        
        // Show the file content
        display_write(0, 12, display_buffer, WHITE, BLACK, 1);
        
        // Add scroll indicators if needed
        if (start_line > 0) {
            display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
        }
        
        // Estimate if there's more content below
        if (file_pos + strlen(display_buffer) < file->n_data) {
            display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
        }
        
        // Add navigation help at bottom
        display_write(0, SSD1306_HEIGHT - 8, "<:Back ^v:Scroll", WHITE, BLACK, 1);
        
        display_update();
        trace("contents of display buffer: %s", display_buffer);

        while(gpio_read(input_left) && gpio_read(input_right) && 
              gpio_read(input_top) && gpio_read(input_bottom) && 
              gpio_read(input_single)) {
            // Just wait for a button press
            delay_ms(50);
        }
        
        // Check for button presses and scroll accordingly
        if (!gpio_read(input_left)) {
            // Exit file view immediately
            return;
        }
        
        if (!gpio_read(input_top) && start_line > 0) {
            // Scroll up one line at a time
            start_line--;
            delay_ms(200);
        }
        
        if (!gpio_read(input_bottom) && file_pos + strlen(display_buffer) < file->n_data) {
            // Scroll down one line at a time
            start_line++;
            delay_ms(200);
        }
        
        if (!gpio_read(input_right) && file_pos + strlen(display_buffer) < file->n_data) {
            // Also scroll down one line at a time
            start_line++;
            delay_ms(200);
        }
        
        // Wait for button release
        while(!gpio_read(input_left) || !gpio_read(input_right) || 
              !gpio_read(input_top) || !gpio_read(input_bottom) || 
              !gpio_read(input_single)) {
            delay_ms(50);
        }
    }
}

// Extended directory entry with parent pointer for navigation; funky but okay
typedef struct {
    pi_dirent_t entry;         // The actual directory entry
    void* parent;              // Pointer to parent directory entry
} ext_dirent_t;

// Then, update the show_files function to use this extended structure:

void show_files(fat32_fs_t *fs, pi_dirent_t *starting_directory) {
    // Constants for navigation
    const uint32_t NUM_ENTRIES_TO_SHOW = 4; // Show 4 files at a time on screen
    
    // Create an extended directory structure to track parents
    ext_dirent_t current_dir;
    current_dir.entry = *starting_directory;
    current_dir.parent = NULL;  // Root has no parent
    
    // Navigation state variables
    uint32_t top_index = 0;      // First visible entry index
    uint32_t selected_index = 0;  // Currently selected entry
    int entries_offset = 0;       // Offset for skipping special directories
    
    // Main file browser loop
    while(1) {
        // Read current directory contents
        pi_directory_t files = fat32_readdir(fs, &current_dir.entry);
        uint32_t total_entries = files.ndirents;

        printk("Got %d files.\n", files.ndirents);
        
        // Count how many entries start with . to calculate our offset (bc we don't show them)
        entries_offset = 0;
        for (int i = 0; i < total_entries; i++) {
            pi_dirent_t *dirent = &files.dirents[i];
            if (dirent->name[0] == '.') {
                // Skip entries that start with . bc they are funky and somehow refer to cluster 0
                entries_offset++;
            }
        }
        
        // Adjusted total entries (excluding special directories)
        uint32_t adjusted_total = total_entries - entries_offset;
        
        // Handle empty directories (after filtering)
        if (adjusted_total == 0) {
            display_clear();
            display_write(10, 20, "Empty directory", WHITE, BLACK, 1);
            display_update();
            delay_ms(1000);
            continue;
        }
        
        // Reset selection when entering a new directory
        if (selected_index >= adjusted_total) {
            selected_index = 0;
            top_index = 0;
        }
        
        // Calculate visible range
        uint32_t bot_index = min(top_index + NUM_ENTRIES_TO_SHOW - 1, adjusted_total - 1);
        
        // Build the display text
        char text_to_display[18 * NUM_ENTRIES_TO_SHOW + 1]; // Max filename(16) + selector(1) + newline(1) + null(1)
        text_to_display[0] = '\0';
        int text_pos = 0;
        
        // Add visible entries to the display buffer
        int real_index = 0;  // Index into the actual directory entries
        int filtered_index = 0;  // Index into filtered entries (excluding '.' entries)
        
        while (filtered_index <= bot_index && real_index < total_entries) {
            pi_dirent_t *dirent = &files.dirents[real_index];
            
            // Skip entries that start with . (as before)
            if (dirent->name[0] == '.') {
                real_index++;
                continue;
            }
            
            // Only process entries that are within our visible range
            if (filtered_index >= top_index && filtered_index <= bot_index) {
                // Add selection indicator
                if (filtered_index == selected_index) {
                    text_to_display[text_pos++] = '>';
                } else {
                    text_to_display[text_pos++] = ' ';
                }
                
                // If it's a directory, add the slash
                if (dirent->is_dir_p) {
                    text_to_display[text_pos++] = '/';
                }
                
                // Copy filename
                int j = 0;
                // Skip trailing spaces when displaying
                int last_non_space = -1;
                while (dirent->name[j] != '\0' && text_pos < sizeof(text_to_display) - 2) {
                    if (dirent->name[j] != ' ') {
                        last_non_space = j;
                    }
                    text_to_display[text_pos++] = dirent->name[j++];
                }
                
                // Trim trailing spaces in display
                if (last_non_space >= 0 && j > 0) {
                    text_pos = text_pos - (j - last_non_space - 1);
                }
                
                // Add newline
                if (text_pos < sizeof(text_to_display) - 2) {
                    text_to_display[text_pos++] = '\n';
                }
                
                // Ensure null termination
                text_to_display[text_pos] = '\0';
            }
            filtered_index++;
            real_index++;
        }
        
        // Display current directory name and file list
        display_clear();
        
        // Prepare directory name display
        char dir_name[22]; // Limit to screen width
        if (current_dir.entry.name[0] == '\0') {
            safe_strcpy(dir_name, "Root Directory", sizeof(dir_name));
        } else {
            safe_strcpy(dir_name, current_dir.entry.name, sizeof(dir_name));
        }
        
        // Show the current directory at the top
        display_write(10, 0, dir_name, WHITE, BLACK, 1);
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        
        // Show file list
        display_write(0, 12, text_to_display, WHITE, BLACK, 1);
        
        // Show navigation help
        if (current_dir.parent != NULL) {
            display_write(0, SSD1306_HEIGHT - 8, "^v:Move <:Back >:Open", WHITE, BLACK, 1);
        } else {
            display_write(0, SSD1306_HEIGHT - 8, "^v:Move <:Exit >:Open", WHITE, BLACK, 1);
        }
        
        display_update();
        delay_ms(100); // Debouncing delay
        
        // Wait for button input
        while(gpio_read(input_left) && gpio_read(input_right) && 
              gpio_read(input_top) && gpio_read(input_bottom) && 
              gpio_read(input_single)) {
            // Just wait for a button press
            delay_ms(50);
        }
        
        // Handle button input
        if (!gpio_read(input_left)) {
            // This is the "back" button - navigate to parent directory if we have one
            if (current_dir.parent != NULL) {
                display_clear();
                display_write(10, 20, "Going back...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                
                // We need to cast the parent pointer back to ext_dirent_t*
                ext_dirent_t* parent_dir = (ext_dirent_t*)current_dir.parent;
                
                // Navigate to parent directory
                current_dir = *parent_dir;
                selected_index = 0;  // Reset selection
                top_index = 0;       // Reset view position
            } else {
                // At root, so exit the file browser
                trace("exiting file system");
                return;
            }
            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_bottom)) {
            // Move selection down
            if (selected_index < adjusted_total - 1) {
                selected_index++;
                
                // Scroll if selection goes below visible area
                if (selected_index > bot_index) {
                    top_index++;
                }
            }
            delay_ms(200); // Prevent rapid scrolling
        }
        else if (!gpio_read(input_right)) {
            // Convert selected_index to real index
            int real_selected_index = 0;
            int count = 0;
            
            for (int i = 0; i < total_entries; i++) {
                if (files.dirents[i].name[0] == '.') {
                    continue;  // Skip entries that start with .
                }
                
                if (count == selected_index) {
                    real_selected_index = i;
                    break;
                }
                count++;
            }
            
            // Get the selected directory entry
            pi_dirent_t *selected_dirent = &files.dirents[real_selected_index];
            
            // Handle selection action based on entry type
            if (selected_dirent->is_dir_p) {
                // Create a new extended directory entry with parent pointer
                ext_dirent_t* new_parent = kmalloc(sizeof(ext_dirent_t));
                
                // Copy the current directory to be used as parent
                *new_parent = current_dir;
                
                // Create a new current directory with the selected entry
                current_dir.entry = *selected_dirent;
                current_dir.parent = new_parent;
                
                // Regular directory - navigate into it
                display_clear();
                display_write(10, 20, "Entering directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                
                selected_index = 0;  // Reset selection for new directory
                top_index = 0;       // Reset view position
            } 
            else {
                // It's a file - display its content
                display_file(fs, &current_dir.entry, selected_dirent);
            }
            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_top)) { // for aditi hardware
            // Move selection up
            if (selected_index > 0) {
                selected_index--;
                
                // Scroll if selection goes above visible area
                if (selected_index < top_index) {
                    top_index--;
                }
            }
            delay_ms(200); // Prevent rapid scrolling
        }
        else if (!gpio_read(input_single)) {
            // TODO: ADITI IS THIS WHAT SINGLE SHOULD DO???

            show_menu(fs, &current_dir.entry);
                // Create a new file in current directory
            // display_clear();
            // display_write(5, 20, "Creating new file... (not implemented)", WHITE, BLACK, 1);
            // display_update();
            // delay_ms(500);
            
            delay_ms(200); // Debounce
        }
    }
}






void notmain(void) {
    trace("Starting Button + Display Integration Test\n");

    // display init 
    trace("Initializing display at I2C address 0x%x\n", SSD1306_I2C_ADDR);
    display_init();
    trace("Display initialized\n");
    
    // Clear the display
    display_clear();
    display_update();
    trace("Display cleared\n");

    // button init
    trace("Initializing buttons\n");

    // do in the middle in case some interference
    gpio_set_input(input_single);
    gpio_set_input(input_right);
    gpio_set_input(input_bottom);
    gpio_set_input(input_top);
    gpio_set_input(input_left);
    trace("buttons initialized\n");
    //test_buttons();

    // Initialize FAT32 filesystem
    trace("Starting FAT shenanigans\n");
    kmalloc_init(FAT32_HEAP_MB);
    pi_sd_init();
  
    trace("Reading the MBR\n");
    mbr_t *mbr = mbr_read();
  
    trace("Loading the first partition\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));
  
    trace("Loading the FAT\n");
    fat32_fs_t fs = fat32_mk(&partition);
  
    trace("Loading the root directory\n");
    pi_dirent_t root = fat32_get_root(&fs);
  
    // Display welcome screen with bouncing Pi symbol animation
    int bounce_x = SSD1306_WIDTH / 2;
    int bounce_y = 30;
    int dx = 1;
    int dy = 1;
    int frame = 0;
    
    while (gpio_read(input_single) && gpio_read(input_right) && 
           gpio_read(input_bottom) && gpio_read(input_top) && 
           gpio_read(input_left)) {
        
        display_clear();
        
        display_write(8, 2, "SUZITI File Browser", WHITE, BLACK, 1);
        display_draw_line(0, 12, SSD1306_WIDTH, 12, WHITE);
        
        // Draw a cute <3 folder icon using primitive shapes
        display_fill_rect(bounce_x - 7, bounce_y - 4, 14, 10, WHITE);
        display_fill_rect(bounce_x - 5, bounce_y - 6, 10, 2, WHITE);
        display_fill_rect(bounce_x - 6, bounce_y - 3, 12, 8, BLACK);
        display_fill_rect(bounce_x - 4, bounce_y - 2, 8, 2, WHITE);
        display_fill_rect(bounce_x - 4, bounce_y + 1, 8, 2, WHITE);
        display_fill_rect(bounce_x - 4, bounce_y + 4, 8, 2, WHITE);
        
        if (frame % 30 < 15) {
            display_write(10, 48, "Press any button", WHITE, BLACK, 1);
        }
        display_update();
        
        // bouncing effect
        bounce_x += dx;
        bounce_y += dy;
        
        if (bounce_x <= 10 || bounce_x >= SSD1306_WIDTH - 10) {
            dx = -dx;
        }
        if (bounce_y <= 20 || bounce_y >= 40) {
            dy = -dy;
        }
        
        frame++;
        delay_ms(50);  // Control animation speed
    }
    
    delay_ms(200); // Debounce
    
    // Start file browser
    show_files(&fs, &root);
    
    // Exit message
    display_clear();
    display_write(10, 20, "File Browser Exited", WHITE, BLACK, 1);
    display_update();
    delay_ms(2000);
}
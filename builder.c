#include "builder.h"

f_Collection *build_collection(const char* directory_path, int folder_current_capacity) {
    f_Collection *collection = (f_Collection*)malloc(sizeof(f_Collection));

    collection->folder_count = 0;
    collection->folder_capacity = folder_current_capacity;
    collection->folders = (n_Folder**)malloc(sizeof(n_Folder*) * collection->folder_capacity);


    // INSIDE LOOP
    // run trough the directory and for each folder
    //create a new n_Folder and add it to the collection
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        free(collection->folders);
        free(collection);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 &&
    strcmp(entry->d_name, "..") != 0) {

        if (collection->folder_count == collection->folder_capacity) {
            collection->folder_capacity *= 2;
            collection->folders = 
            (n_Folder**)realloc(collection->folders, collection->folder_capacity * sizeof(n_Folder*));
        }
        // full_path = directory_path + "/" + entry->d_name
        n_Folder *curr = create_folder(entry->d_name, directory_path);
        collection->folders[collection->folder_count] = curr;
        collection->folder_count++;

    }
    }
    closedir(dir);

    // END LOOP
    return collection;
}

int main() {
    f_Collection *col = build_collection("./collections", 5);
    for (int i = 0; i < col->folder_count; i++) {
    free(col->folders[i]);}
    free(col->folders);
    free(col);
    return 0;

}
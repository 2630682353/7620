#ifndef _UCI_FN_H
#define _UCI_FN_H


/*
 * export(print) a config file
 */
int
uuci_print_file(char *file);
/*
 * Set the value of the given option, 
 * or add a new section with the type 
 * set to the given value.
 * 
 * eg: s = "network.@interface[0].ipaddr=192.168.1.1"
 */
int
uuci_set(char *s);

/*
 * Add an anonymous section of type section-type
 * to the given configuration.
 *
 * eg: s = "test"
 */
int
uuci_add(char *file, char *s);

/*
 * Add the given string to an existing list option.
 *
 * eg: s = "network.@test[0].index=some"
 */
int
uuci_add_list(char *s);

/*
 * delete a list
 *
 * eg: s = "network.@test[0].index=some"
 */
int
uuci_del_list(char *s);
/*
 * delete the given section or option
 *
 * eg: s = "network.@test[0]"
 */
int
uuci_delete(char *s);
/*
 * Get the value of the given option or
 * the type of the given section.
 * 
 * num: numbers of all strings
 * char **array: returned strings stored in
 * s = "network.@interface[0].ipaddr"
 *
 * caller need free the array
 */
int
uuci_get(char *s, char ***array, int *num);
/*
 * free the returned array from uuci_get()
 */
int
uuci_get_free(char **array, int num);
/*
 *Writes changes of the given configuration file, 
 */
int
uuci_commit(char *s);






#endif










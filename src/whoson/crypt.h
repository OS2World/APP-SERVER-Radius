/* header information for crypt.dll */

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * first parameter:   string to be encrypted
 * second parameter:  string to use as key
 * return value:      pointer to static encrypted string
 *                    valid only until next call
 *
 */

char *crypt(const char*, const char*);

#ifdef __cplusplus
}
#endif
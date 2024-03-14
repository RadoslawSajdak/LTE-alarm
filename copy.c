#define LOC                         "50.2091,19.251"
#define URL_HEADER                  "https://www.google.com/maps/place/"
int main(void)
{
    char url[sizeof(URL_HEADER) + sizeof(LOC)] = {0};
    memcpy(url, URL_HEADER, sizeof(URL_HEADER));
    memcpy(url + sizeof(URL_HEADER) - 1, LOC, sizeof(LOC));
    printf("%s\n", url);
}

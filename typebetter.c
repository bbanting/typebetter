#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>


#define REQUEST_FAILURE 1
#define REQUEST_SUCCESS 0
#define API_URL "https://api.esv.org/v3/passage/text/?q=John+3&include-headings=False&include-footnotes=False&include-verse-numbers=True&include-short-copyright=False&include-passsage-references=False&include-first-verse-numbers=False"


typedef struct {
	char* contents;
	size_t size;
} Response;


/**Callback to be used by curl. */
static size_t write_callback(char *data, size_t size, size_t nmemb, void *data_dest) {
	Response *res = (Response *)data_dest;
	char *ptr = realloc(res->contents, res->size + nmemb + 1);
	if (ptr == NULL) {
		fprintf(stderr, "realloc failed.\n");
		return 0; // Failure
	}

	res->contents = ptr;
	memcpy(&(res->contents[res->size]), data, nmemb);
	res->size += nmemb;
	res->contents[res->size] = 0;

	return nmemb; // Success
}


/**Get authorization token from file and construct full header string,
 * copy to buffer (str).
*/
static void get_auth_header(char *str) {
	FILE *fp = fopen("token.txt", "r");
	if (fp == NULL) {
		fprintf(stderr, "Error opening token file.\n");
		exit(EXIT_FAILURE);
	}

	char token[100];
	fgets(token, 100, fp);
	token[strlen(token) - 1] = 0;

	strcat(str, "Authorization: ");
	strcat(str, token);
} 


/**Takes a url and pointer to Response struct, makes request and assigns
 * the response body to res.content. Non-zero return values indicate failure.
*/
int make_request(const char *url, Response *res) {
	// Initialize curl.
	CURL *curl;
	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Curl init failure.\n");
		return REQUEST_FAILURE;
	}

	// Construct headers.
	char auth_header[100];
	get_auth_header(auth_header);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, auth_header);

	if (headers == NULL) return REQUEST_FAILURE;

	// Set curl options.
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);

	// Make request with curl.
	CURLcode res_code = curl_easy_perform(curl);

	if (res_code != CURLE_OK) {
		fprintf(stderr, "Curl failure: %s\n", curl_easy_strerror(res_code));
		free(res->contents);
		return REQUEST_FAILURE;
	}

	// Clean up.
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return REQUEST_SUCCESS;
}


/**Take dummy response data from a text file and use it to populate
 * res. Same interface as make_request except it expects a filepath instead
 * of a URL. 
*/
int make_dummy_request(const char *filepath, Response *res) {
	// Open the file.
	FILE *fp = fopen(filepath, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open dummy response file.\n");
		return REQUEST_FAILURE;
	}

	// Loop through file contents and copy text to res->contents.
	// The size of res->contents is enlarged gradually.
	res->size = 50;
	res->contents = malloc(res->size);
	unsigned long count = 0;
	int ch;
	while ((ch = getc(fp)) != EOF) {
		if ((++count + 1) <= res->size) {
			// Enough space for ch.
			res->contents[count-1] = (char)ch;
			continue;
		}
		// Not enough space for ch. Enlarge array.
		res->size += 50;
		char *ptr = realloc(res->contents, res->size);
		if (ptr == NULL) {
			fprintf(stderr, "Realloc failed.\n");
			return REQUEST_FAILURE;
		}
		res->contents = ptr;
		res->contents[count-1] = (char)ch;
	}

	res->contents[count] = 0;

	return REQUEST_SUCCESS;
}


/**Clean up any memory contained in a Response.*/
void cleanup_response(Response *res) {
	free(res->contents);
}


/**Return a string containing the verse referenced by n. If the verse
 * is not found, return NULL. 
*/
char *get_nth_verse(const char *text, unsigned n) {
	// Get start and end indexes.
	char search[10];

	sprintf(search, "[%d]", n);
	char *start = strstr(text, search);
	if (start == NULL) {
		return NULL;
	} else {
		start += (strlen(search) + 1);
	}

	sprintf(search, "[%d]", n+1);
	char *end = strstr(text, search);
	if (end == NULL) {
		end = (text + strlen(text));
	} else {
		end -= 1;
	}

	// Allocate memory for verse.
	char *str;
	if ((str = malloc((end-start)+1)) == NULL) {
		fprintf(stderr, "Malloc error in get_nth_verse\n");
		exit(EXIT_FAILURE);
	}

	// Copy verse string into str.
	for (int i=0; i<end-start; i++) {
		str[i] = start[i];
	}
	str[end-start] = 0;

	return str;
}


int main(int argc, char *argv[]) {
	// Get verse number from input.
	// unsigned verse_number = 1;
	// if (argc > 1) {
	// 	verse_number = strtol(argv[1], NULL, 10);
	// 	if (verse_number > 200 || verse_number < 1)
	// 		fprintf(stderr, "Enter a valid verse number.\n");
	// } else {
	// 	fprintf(stderr, "Invalid command format. Enter a verse number.\n");
	// 	exit(EXIT_FAILURE);
	// }


	// Make request.
	Response response = {0};
	// if (make_request(API_URL, &response) == REQUEST_FAILURE) {
	// 	exit(EXIT_FAILURE);
	// }
	if (make_dummy_request("dummy_response.txt", &response) == REQUEST_FAILURE) {
		exit(EXIT_FAILURE);
	}


	// The main loop.
	for (int n=1; 1; n++) {
		char *verse = get_nth_verse(response.contents, n);
		if (verse == NULL) {
			break;
		}
		printf("Verse %u: %s\n", n, verse);
		free(verse);
	}


	cleanup_response(&response);

	return EXIT_SUCCESS;
}

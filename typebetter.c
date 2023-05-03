#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>


#define REQUEST_FAILURE 1
#define REQUEST_SUCCESS 0
#define API_URL "https://api.esv.org/v3/passage/text/?q=John+3&include-headings=False&include-footnotes=False&include-verse-numbers=False&include-short-copyright=False&include-passsage-references=False&include-first-verse-numbers=False"


typedef struct response_data {
	char* contents;
	size_t size;
} Response;


static size_t write_callback(char *data, size_t size, size_t nmemb, void *data_memory) {
	struct response_data *rd = (struct response_data *)data_memory;
	char *ptr = realloc(rd->contents, rd->size + nmemb + 1);
	if (ptr == NULL) {
		fprintf(stderr, "realloc failed.\n");
		return 0; // Failure
	}

	rd->contents = ptr;
	memcpy(&(rd->contents[rd->size]), data, nmemb);
	rd->size += nmemb;
	rd->contents[rd->size] = 0;

	return nmemb; // Success
}


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


int main() {
	Response response = {0};
	if (make_request(API_URL, &response) == REQUEST_FAILURE) {
		exit(EXIT_FAILURE);
	}

	printf("Response contents: %s\n", response.contents);

	free(response.contents);

	return EXIT_SUCCESS;
}

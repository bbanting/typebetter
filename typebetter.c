#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define API_URL "https://api.esv.org/v3/passage/text/?q=John+3%3A16&include-headings=False&include-footnotes=False&include-verse-numbers=False&include-short-copyright=False&include-passsage-references=False&include-first-verse-numbers=False"


struct response_data {
	char* contents;
	size_t size;
};


size_t write_callback(char *data, size_t size, size_t nmemb, void *data_memory) {
	struct response_data *rd = (struct response_data *)data_memory;
	char *ptr = realloc(rd->contents, rd->size + nmemb + 1);
	if (ptr == NULL) {
		fprintf(stderr, "realloc failed.\n");
		return 0;
	}
	rd->contents = ptr;
	memcpy(&(rd->contents[rd->size]), data, nmemb);
	rd->size += nmemb;
	rd->contents[rd->size] = 0;

	return nmemb;
}


void get_auth_header(char *str) {
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


int main() {
	CURL *curl;
	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Curl init failure.\n");
		return EXIT_FAILURE;
	}

	CURLcode res_code;

	struct response_data res_data = {0};
	struct curl_slist *headers = NULL;

	char auth_header[100];
	get_auth_header(auth_header);
	headers = curl_slist_append(headers, auth_header);

	if (headers == NULL) {
		return EXIT_FAILURE;
	}
	
	curl_easy_setopt(curl, CURLOPT_URL, API_URL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&res_data);

	res_code = curl_easy_perform(curl);

	if (res_code != CURLE_OK) {
		fprintf(stderr, "Curl failure: %s\n", curl_easy_strerror(res_code));
		return EXIT_FAILURE;
	}

	printf("Here's the response data: %s\n", res_data.contents);
		
	
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	free(res_data.contents);

	return EXIT_SUCCESS;
}

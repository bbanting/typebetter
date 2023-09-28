#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>
#include <unistd.h>


#define REQUEST_FAILURE 1
#define REQUEST_SUCCESS 0
#define API_URL "https://api.esv.org/v3/passage/text/?q=John+3&include-headings=False&include-footnotes=False&include-verse-numbers=True&include-short-copyright=False&include-passsage-references=False&include-first-verse-numbers=False"


typedef struct Response {
	char* contents;
	size_t size;
} Response;


typedef struct Verse {
	unsigned number;
	char *text;
} Verse;


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


/**
 * Takes a string with an offset that points to a unicode escape sequence 
 * and returns the char it references. Exits with failure if the sequence 
 * isn't found.
 */
char get_char_from_unicode_escape_sequence(const char *string, unsigned offset) {
	unsigned num_targets = 5;
	char targets[5][4] = {"2014", "2018", "2019", "201c", "201d"};
	char replacements[5] = {'-', '\'', '\'', '\"', '\"'};

	for (int i = 0; i < num_targets; i++) {
		if (strncmp(string+offset, targets[i], 4) == 0) {
			return replacements[i];
		}
	}

	// No matches, report error.
	char sequence[5];
	strncpy(sequence, string+offset, 4);
	sequence[5] = '\0';
	fprintf(stderr, "No match for the unicode escape sequence: %s\n", sequence);
	exit(EXIT_FAILURE);
}


/**Replace unicode escape sequences with appropriate characters. */
void replace_escape_sequences(char *text) {
	char new_text[strlen(text)+1];
	unsigned new_len = 0;

	int i = 0;
	while (i < strlen(text)-1) {
		if (text[i] == '\\' && text[i+1] == 'u') {
			// It's a unicode escape sequence.
			new_text[new_len++] = get_char_from_unicode_escape_sequence(text, i+2);
			i += 6;
		} else if (text[i] == '\\' && text[i+1] == 'n') {
			// It's a newline escape sequence.
			i += 2;
		} else {
			// It's a normal character.
			new_text[new_len++] = text[i];
			i += 1;
		}
	}
	new_text[new_len] = '\0';

	strcpy(text, new_text);
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
char *get_nth_verse_text(const char *text, unsigned n) {
	#define TRAILING_CHARS 6

	char search[10];
	char *start;
	char *end;

	// Get the start index for the verse text.
	sprintf(search, "[%d]", n);
	start = strstr(text, search);
	if (start == NULL) {
		return NULL;
	} else {
		start += (strlen(search) + 1);
	}

	// Get the end index for the verse text.
	char *lbracket = strstr(start, "[");
	char *rbracket = strstr(start, "]");
	if (lbracket == NULL || rbracket == NULL) {
		end = start + strlen(start) - TRAILING_CHARS; 
	} else {
		end = lbracket - 1;
		for (int i = 1; i < rbracket - lbracket; i++) {
			if (!isdigit(lbracket[i])) {
				end = start + strlen(start) - TRAILING_CHARS;
				break;
			}
		}
	}

	// Allocate memory for verse text and copy.
	char *str;
	if ((str = malloc((end-start)+1)) == NULL) {
		fprintf(stderr, "Malloc error in get_nth_verse_text\n");
		exit(EXIT_FAILURE);
	}
	strncpy(str, start, end - start);
	str[end-start] = '\0';

	return str;
}


/**Get the number of the last verse in the chapter. */
unsigned get_last_verse_number(const char *text) {
	// These are values very specific to the ESV API.
	#define START_STR "\"parsed\": [["
	#define LENGTH 18

	// Copy the revevant substring into num_str.
	char *start = strstr(text, START_STR) + strlen(START_STR);
	char num_str[20];
	strncpy(num_str, start, LENGTH);
	num_str[LENGTH] = '\0';

	// Scan integers from that string into first and last.
	unsigned first, last;
	sscanf(num_str, "%u, %u", &first, &last);

	// Turn last back into a string and scan it back into an integer, 
	// ignoring the irrelevant characters.
	char last_str[10];
	sprintf(last_str, "%u", last);
	sscanf(last_str + 5, "%u", &last);

	return last;
}


/**Copy verses from text into array. */
Verse *get_verses(const char *text, unsigned *num_verses) {
	size_t size = 20;
	Verse *verses = malloc(sizeof(Verse) * size);
	unsigned last_verse_number = get_last_verse_number(text);

	for (unsigned i = 1; i <= last_verse_number; i++) {
		char *verse_text = get_nth_verse_text(text, i);
		if (verse_text == NULL) continue;

		// Check if verses is full. If so, expand.
		if (*num_verses == size) {
			size += 10;
			Verse *ptr = realloc(verses, sizeof(Verse) * size);
			if (ptr == NULL) {
				fprintf(stderr, "realloc error in get_verses.\n");
				exit(EXIT_FAILURE);
			}
			verses = ptr;
		}

		// Make and append the verse.
		Verse verse = {i, verse_text};
		verses[*num_verses] = verse;
		*num_verses += 1;
	}

	return verses;
}


/**Free the memory used by verses. */
void cleanup_verses(Verse *verses, unsigned n) {
	for (int i = 0; i < n; i++) {
		free(verses[i].text);
	}
	free(verses);
}


/**
 * Prompt the user to attempt to type the string.
 * Returns 1 on user success, 0 on failure.
 */
unsigned make_attempt(char *verse_text, unsigned *t) {
	char attempt[512];

	time_t start = time(NULL);
	fgets(attempt, 512, stdin);
	time_t finish = time(NULL);

	attempt[strlen(attempt)-1] = '\0'; // Remove newline.
	*t = finish - start;

	if (!strcmp(verse_text, attempt)) {
		return 1;
	} else {
		return 0;
	}
}


int main(int argc, char *argv[]) {
	// Make request.
	Response response = {0};
	// if (make_request(API_URL, &response) == REQUEST_FAILURE) {
	// 	exit(EXIT_FAILURE);
	// }
	if (make_dummy_request("dummy_response.txt", &response) == REQUEST_FAILURE) {
		exit(EXIT_FAILURE);
	}

	replace_escape_sequences(response.contents);

	unsigned num_verses = 0;
	Verse *verses = get_verses(response.contents, &num_verses);
	unsigned total_time = 0;
	unsigned successful_time = 0;
	unsigned successes = 0;

	// Main loop.
	for (unsigned i=0; i < num_verses; i++) {
		unsigned t;
		printf("Verse %u (%u/%u):\n%s\n", verses[i].number, i+1, num_verses, verses[i].text);

		if (make_attempt(verses[i].text, &t) > 0) {
			printf("You did it (%us).\n\n", t);
			successes += 1;
			successful_time += t;
		} else {
			printf("You didn't do it.\n\n");
		}

		total_time += t;
		sleep(1);
	}

	printf("This chapter took you %us total to complete.\n", total_time);

	// Cleanup.
	cleanup_response(&response);
	cleanup_verses(verses, num_verses);

	return EXIT_SUCCESS;
}

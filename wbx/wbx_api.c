#include <stdio.h>
#include <stdlib.h>

#include "wbx_api.h"

void* wbx_api_get_data_or_abort(wbx_api_return_data_t* return_data) {
	if (*return_data->error_message) {
		fprintf(stderr, "%s", return_data->error_message);
		abort();
	}

	return return_data->data;
}

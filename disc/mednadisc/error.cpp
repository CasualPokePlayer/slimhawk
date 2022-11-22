/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "slim_types.h"
#include "error.h"

MDFN_Error::MDFN_Error() noexcept
{
 abort();
}

MDFN_Error::MDFN_Error(int errno_code_new, const char *format, ...) noexcept
{
 errno_code = errno_code_new;

 error_message = NULL;

 int size = 128;
 for(;;) {
	va_list ap;
	va_start(ap, format);
	error_message = (char*)malloc(size);
	size *= 2;
	int ret = vsprintf(error_message, format, ap);
	va_end(ap);
	if(ret>=0)
		break;
	free(error_message);
 }
}


MDFN_Error::MDFN_Error(const ErrnoHolder &enh)
{
 errno_code = enh.Errno();

 int size = 128;
 for(;;) {
	error_message = (char*)malloc(size);
	size *= 2;
	int ret = sprintf("%s", enh.StrError());
	if(ret>=0)
		break;
	free(error_message);
 }
}


MDFN_Error::~MDFN_Error() noexcept
{
 if(error_message)
 {
  free(error_message);
  error_message = NULL;
 }
}

MDFN_Error::MDFN_Error(const MDFN_Error &ze_error) noexcept
{
 if(ze_error.error_message)
  error_message = strdup(ze_error.error_message);
 else
  error_message = NULL;

 errno_code = ze_error.errno_code;
}

MDFN_Error& MDFN_Error::operator=(const MDFN_Error &ze_error) noexcept
{
 char *new_error_message = ze_error.error_message ? strdup(ze_error.error_message) : NULL;
 int new_errno_code = ze_error.errno_code;

 if(error_message)
  free(error_message);

 error_message = new_error_message;
 errno_code = new_errno_code;

 return(*this);
}


const char * MDFN_Error::what(void) const noexcept
{
 if(!error_message)
  return("Error allocating memory for the error message!");

 return(error_message);
}

int MDFN_Error::GetErrno(void) const noexcept
{
 return(errno_code);
}

static const char *srr_wrap(int ret, const char *local_strerror)
{
 if(ret == -1)
  return("ERROR IN strerror_r()!!!");

 return(local_strerror);
}

static const char *srr_wrap(const char *ret, const char *local_strerror)
{
 if(ret == NULL)
  return("ERROR IN strerror_r()!!!");

 return(ret);
}

void ErrnoHolder::SetErrno(int the_errno)
{
 local_errno = the_errno;

 if(the_errno == 0)
  local_strerror[0] = 0;
 else
 {
  #ifdef HAVE_STRERROR_R
   const char *retv;

   retv = srr_wrap(strerror_r(the_errno, local_strerror, 256), local_strerror);

   if(retv != local_strerror)
    strncpy(local_strerror, retv, 255);

  #else	// No strerror_r :(

   strncpy(local_strerror, strerror(the_errno), 255);

  #endif

  local_strerror[255] = 0;
 }
}


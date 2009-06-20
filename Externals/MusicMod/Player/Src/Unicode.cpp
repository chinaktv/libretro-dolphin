////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright � 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "Unicode.h"



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ToAnsi( char * szDest, wchar_t * szSource, int iLen )
{
	char * const szBytesource = ( char * )szSource;
	for( int i = 0; i < iLen; i++ )
	{
		szDest[ i ] = szBytesource[ 2 * i + 1 ];
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
/*
void ToUnicode( wchar_t * szDest, char * szSource, int iLen )
{
	for( int i = 0; i < iLen; i++ )
	{
		szDest[ i ] = ( wchar_t )szSource[ i ];
	}
}
*/



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ToTchar( TCHAR * szDest, wchar_t * szSource, int iLen )
{
#ifdef PA_UNICODE
	memcpy( szDest, szSource, 2 * iLen );
#else
	char * const stByteSource = ( TCHAR * )szSource;
	for( int i = 0; i < iLen; i++ )
	{
		szDest[ i ] = stByteSource[ 2 * i + 1 ];
	}
#endif
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ToTchar( TCHAR * szDest, char * szSource, int iLen )
{
#ifdef PA_UNICODE
	for( int i = 0; i < iLen; i++ )
	{
		szDest[ i ] = szSource[ 2 * i + 1 ];
	}
#else
	memcpy( szDest, szSource, iLen );
#endif
}

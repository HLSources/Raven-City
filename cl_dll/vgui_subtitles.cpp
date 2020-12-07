// ====================================
// Paranoia subtitle system interface
//		written by BUzer
// ====================================

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "vgui_TeamFortressViewport.h"
#include "VGUI_TextImage.h"

Font* FontFromMessage(const char* &ptext)
{
	char fontname[64] = "Default Text";
	if (ptext != NULL && ptext[0] != 0)
	{
		if (ptext[0] == '@')
		{
			// get font name
			ptext++;
			ptext = gEngfuncs.COM_ParseFile((char*)ptext, fontname);
			ptext+=2;
		}
	}

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTextScheme = pSchemes->getSchemeHandle( fontname );
	return pSchemes->getFont( hTextScheme );
}
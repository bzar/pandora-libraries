
#include "tinyxml/tinyxml.h"
#include "../include/pnd_pxml.h"
#include "pnd_tinyxml.h"

extern "C" {

unsigned char pnd_pxml_load ( const char* pFilename, pnd_pxml_t *app ) {
  FILE *f;
  char *b;
  unsigned int len;

  f = fopen ( pFilename, "r" );

  if ( ! f ) {
    return ( 0 );
  }

  fseek ( f, 0, SEEK_END );

  len = ftell ( f );

  fseek ( f, 0, SEEK_SET );

  b = (char*) malloc ( len );

  if ( ! b ) {
    fclose ( f );
    return ( 0 );
  }

  fread ( b, 1, len, f );

  return ( pnd_pxml_parse ( pFilename, b, len, app ) );
}

unsigned char pnd_pxml_parse ( const char *pFilename, char *buffer, unsigned int length, pnd_pxml_t *app ) {

  //TiXmlDocument doc(pFilename);
  //if (!doc.LoadFile()) return;

  TiXmlDocument doc;

  doc.Parse ( buffer );

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem;
	TiXmlHandle hRoot(0);

	pElem=hDoc.FirstChildElement().Element();
	if (!pElem) return ( 0 );
	hRoot=TiXmlHandle(pElem);

	pElem = hRoot.FirstChild( "title" ).FirstChildElement("en").Element();
	if ( pElem )
	{
		app->title_en = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "title" ).FirstChildElement("de").Element();
	if ( pElem )
	{
		app->title_de = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "title" ).FirstChildElement("it").Element();
	if ( pElem )
	{
		app->title_it = strdup(pElem->GetText());
	}
	
	pElem = hRoot.FirstChild( "title" ).FirstChildElement("fr").Element();
	if ( pElem )
	{
		app->title_fr = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("unique_id").Element();
	if (pElem)
	{	
		app->unique_id = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("standalone").Element();
	if (pElem)
	{	
		app->standalone = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("icon").Element();
	if (pElem)
	{	
		char anotherbuffer [ FILENAME_MAX ];
		strcpy ( anotherbuffer, pFilename );
		char *s = strstr ( anotherbuffer, PXML_FILENAME );
		if ( s ) {
		  strcpy ( s, strdup(pElem->GetText()));
		  app->icon = strdup(anotherbuffer);
		}
	}

	pElem = hRoot.FirstChild( "description" ).FirstChildElement("en").Element();
	if ( pElem )
	{
		app->description_en = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "description" ).FirstChildElement("de").Element();
	if ( pElem )
	{
		app->description_de = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "description" ).FirstChildElement("it").Element();
	if ( pElem )
	{
		app->description_it = strdup(pElem->GetText());
	}
	
	pElem = hRoot.FirstChild( "description" ).FirstChildElement("fr").Element();
	if ( pElem )
	{
		app->description_fr = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "previewpic" ).FirstChildElement("pic1").Element();
	if ( pElem )
	{
		app->previewpic1 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "previewpic" ).FirstChildElement("pic2").Element();
	if ( pElem )
	{
		app->previewpic2 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "author" ).FirstChildElement("name").Element();
	if ( pElem )
	{
		app->author_name = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "author" ).FirstChildElement("website").Element();
	if ( pElem )
	{
		app->author_website = strdup(pElem->GetText());;
	}

	pElem = hRoot.FirstChild( "version" ).FirstChildElement("major").Element();
	if ( pElem )
	{
		app->version_major = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "version" ).FirstChildElement("minor").Element();
	if ( pElem )
	{
		app->version_minor = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "version" ).FirstChildElement("release").Element();
	if ( pElem )
	{
		app->version_release = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "version" ).FirstChildElement("build").Element();
	if ( pElem )
	{
		app->version_build = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("exec").Element();
	if (pElem)
	{	
		char anotherbuffer [ FILENAME_MAX ];
		strcpy ( anotherbuffer, pFilename );
		char *s = strstr ( anotherbuffer, PXML_FILENAME );
		if ( s ) {
		  strcpy ( s, strdup(pElem->GetText()));
		  app->exec = strdup(anotherbuffer);
		} else if ( ( s = strrchr ( anotherbuffer, '/' ) ) ) {
		  s += 1;
		  strcpy ( s, strdup(pElem->GetText()));
		  app->exec = strdup(anotherbuffer);
		}
	}	

	pElem = hRoot.FirstChild( "category" ).FirstChildElement("main").Element();
	if ( pElem )
	{
		app->main_category = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "category" ).FirstChildElement("subcategory1").Element();
	if ( pElem )
	{
		app->subcategory1 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "category" ).FirstChildElement("subcategory2").Element();
	if ( pElem )
	{
		app->subcategory2 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "altcategory" ).FirstChildElement("main").Element();
	if ( pElem )
	{
		app->altcategory = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "altcategory" ).FirstChildElement("subcategory1").Element();
	if ( pElem )
	{
		app->altsubcategory1 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "altcategory" ).FirstChildElement("subcategory2").Element();
	if ( pElem )
	{
		app->altsubcategory2 = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "osversion" ).FirstChildElement("major").Element();
	if ( pElem )
	{
		app->osversion_major = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "osversion" ).FirstChildElement("minor").Element();
	if ( pElem )
	{
		app->osversion_minor = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "osversion" ).FirstChildElement("release").Element();
	if ( pElem )
	{
		app->osversion_release = strdup(pElem->GetText());
	}	

	pElem = hRoot.FirstChild( "osversion" ).FirstChildElement("build").Element();
	if ( pElem )
	{
		app->osversion_build = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem1" ).FirstChildElement("name").Element();
	if ( pElem )
	{
		app->associationitem1_name = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem1" ).FirstChildElement("filetype").Element();
	if ( pElem )
	{
		app->associationitem1_filetype = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem1" ).FirstChildElement("parameter").Element();
	if ( pElem )
	{
		app->associationitem1_parameter = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem2" ).FirstChildElement("name").Element();
	if ( pElem )
	{
		app->associationitem2_name = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem2" ).FirstChildElement("filetype").Element();
	if ( pElem )
	{
		app->associationitem2_filetype = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem2" ).FirstChildElement("parameter").Element();
	if ( pElem )
	{
		app->associationitem2_parameter = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem3" ).FirstChildElement("name").Element();
	if ( pElem )
	{
		app->associationitem3_name = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem3" ).FirstChildElement("filetype").Element();
	if ( pElem )
	{
		app->associationitem3_filetype = strdup(pElem->GetText());
	}

	pElem = hRoot.FirstChild( "associationitem3" ).FirstChildElement("parameter").Element();
	if ( pElem )
	{
		app->associationitem3_parameter = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("clockspeed").Element();
	if (pElem)
	{	
		app->clockspeed = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("background").Element();
	if (pElem)
	{	
		app->background = strdup(pElem->GetText());
	}

	pElem=hRoot.FirstChild("startdir").Element();
	if (pElem)
	{	
		app->startdir = strdup(pElem->GetText());
	}

	return ( 1 );
}

} // extern C

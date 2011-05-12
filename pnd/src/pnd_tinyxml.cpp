
#include <stdio.h>
#include "tinyxml/tinyxml.h"
#include "../include/pnd_pxml.h"
#include "pnd_tinyxml.h"
#include "pnd_logger.h"

//Easily change the tag names if required (globally in this file):
#include "pnd_pxml_names.h"

extern "C" {

char *pnd_pxml_get_attribute(TiXmlElement *elem, const char *name)
{
        const char *value = elem->Attribute(name);
        if (value)
                return strdup(value);
        else
                return NULL;
}

unsigned char pnd_pxml_parse_titles(const TiXmlHandle hRoot, pnd_pxml_t *app) {
  TiXmlElement *pElem;
  app->titles_alloc_c = 4;  //TODO: adjust this based on how many titles a PXML usually has. Power of 2.

  app->titles = (pnd_localized_string_t *)malloc(sizeof(pnd_localized_string_t) * app->titles_alloc_c);
  if (!app->titles) return (0); //errno = NOMEM

  // Go through all title tags and load them.
  // - Check if newer style titles sub-block exists; if so, use that.
  //   - if not, fall back to old style
  //     - failing that, crash earth into sun
  if ( (pElem = hRoot.FirstChild(PND_PXML_NODENAME_TITLES).Element()) ) {
    // newer <titles> block

    pElem = pElem -> FirstChildElement ( PND_PXML_ENAME_TITLE );

    while ( pElem ) {

      // handle <title lang="en_US">Program Title</title>
      //

      // parse out the text and lang
      char *text, *lang;

      if ( ! ( text = strdup ( pElem -> GetText() ) ) ) {
        continue;
      }

      if ( ! ( lang = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_TITLELANG ) ) ) {
        continue;
      }

      // increment counter; if we're running out of buffers, grow to handle the new strings
      app -> titles_c++;

      if ( app -> titles_c > app -> titles_alloc_c ) {
        // we don't have enough strings allocated
        app -> titles_alloc_c <<= 1;
        app -> titles = (pnd_localized_string_t *)realloc((void*)app->titles, app->titles_alloc_c);
        if (!app->titles) return (0); //errno = ENOMEM
      }

      // populate the stringbuf
      pnd_localized_string_t *title = &app->titles[app->titles_c - 1];
      title->language = lang;
      title->string = text;

      // next
      pElem = pElem -> NextSiblingElement ( PND_PXML_ENAME_TITLE );

    } // foreach

  } else {
    // older style <title> entry series

    for ( pElem = hRoot.FirstChild(PND_PXML_ENAME_TITLE).Element(); pElem;
          pElem = pElem->NextSiblingElement(PND_PXML_ENAME_TITLE))
    {

      if ( ! pElem->GetText() ) {
        continue;
      }

      char *text = strdup(pElem->GetText());
      if (!text) continue;

      char *lang = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_TITLELANG);
      if (!lang) continue;

      app->titles_c++;
      if (app->titles_c > app->titles_alloc_c) //we don't have enough strings allocated
      {
        app->titles_alloc_c <<= 1;
        app->titles = (pnd_localized_string_t *)realloc((void*)app->titles, app->titles_alloc_c);
        if (!app->titles) return (0); //errno = ENOMEM
      }

      pnd_localized_string_t *title = &app->titles[app->titles_c - 1];
      title->language = lang;
      title->string = text;

      //pnd_log ( PND_LOG_DEFAULT, (char*)"    Title/Lang: %s/%s\n", text, lang );

    } // for

  } // new or old style <title(s)>

  return ( 1 );
}

unsigned char pnd_pxml_parse_descriptions(const TiXmlHandle hRoot, pnd_pxml_t *app) {
  TiXmlElement *pElem;
  app->descriptions_alloc_c = 4; //TODO: adjust this based on how many descriptions a PXML usually has. Power of 2.

  app->descriptions = (pnd_localized_string_t *)malloc(sizeof(pnd_localized_string_t) * app->descriptions_alloc_c);
  if (!app->descriptions)
  {
    app->descriptions_alloc_c = 0;
    return (0); //errno = NOMEM
  }

  // similar logic to how <titles> or <title> is parsed
  // - if <titles> block is found, use that; otherwise fall back to <title> deprecated form
  if ( (pElem = hRoot.FirstChild ( PND_PXML_NODENAME_DESCRIPTIONS).Element() ) ) {
    // newer style <descriptions> block

    pElem = pElem -> FirstChildElement ( PND_PXML_ENAME_DESCRIPTION );

    while ( pElem ) {

      char *text, *lang;

      if ( ! ( text = strdup ( pElem -> GetText() ) ) ) {
        continue;
      }

      if ( ! ( lang = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_DESCRLANG ) ) ) {
        continue;
      }

      app->descriptions_c++;
      if (app->descriptions_c > app->descriptions_alloc_c) //we don't have enough strings allocated
      {
        app->descriptions_alloc_c <<= 1;
        app->descriptions = (pnd_localized_string_t*)realloc((void*)app->descriptions, app->descriptions_alloc_c * sizeof(pnd_localized_string_t) );
        if (!app->descriptions) return (0); //errno = ENOMEM
      }

      pnd_localized_string_t *description = &app->descriptions[app->descriptions_c - 1];
      description->language = lang;
      description->string = text;

      // next
      pElem = pElem -> NextSiblingElement ( PND_PXML_ENAME_DESCRIPTION );

    } // foreach

  } else {
    // fallback to older approach

    for (pElem = hRoot.FirstChild(PND_PXML_ENAME_DESCRIPTION).Element(); pElem;
         pElem = pElem->NextSiblingElement(PND_PXML_ENAME_DESCRIPTION))
    {

      if ( ! pElem->GetText() ) {
        continue;
      }

      char *text = strdup(pElem->GetText());
      if (!text) continue;

      char *lang = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_DESCRLANG);
      if (!lang) continue;

      app->descriptions_c++;
      if (app->descriptions_c > app->descriptions_alloc_c) //we don't have enough strings allocated
      {
        app->descriptions_alloc_c <<= 1;
        app->descriptions = (pnd_localized_string_t*)realloc((void*)app->descriptions, app->descriptions_alloc_c * sizeof(pnd_localized_string_t) );
        if (!app->descriptions) return (0); //errno = ENOMEM
      }

      pnd_localized_string_t *description = &app->descriptions[app->descriptions_c - 1];
      description->language = lang;
      description->string = text;
    } // for

  } // new form or old form?

  return ( 1 );
}

unsigned char pnd_pxml_parse ( const char *pFilename, char *buffer, unsigned int length, pnd_pxml_t **apps ) {

  (void)pFilename;
  (void)length;
  //Load the XML document
  TiXmlDocument doc;
  doc.Parse(buffer);

  unsigned char appwrappermode = 0; // >=1 -> using <application>...</application> wrapper
  unsigned char appcount = 0;
  pnd_pxml_t *app = NULL;

  TiXmlElement *pElem = NULL;
  TiXmlElement *appElem = NULL;

  //Find the root element
  TiXmlHandle hDoc(&doc);
  TiXmlHandle hRoot(0);

  pElem = hDoc.FirstChild("PXML").Element();
  if (!pElem) return (0);

  // new Strategy; really, we want multiple app support within a PXML, without the lameness of
  // multiple <PXML>..</PXML> within a single PXML.xml file. As such, we should have an app within
  // an <application>..</application> wrapper level, with the ID on that. Further, the icon and
  // .desktop filenames can have a # appended which is the application-count-# within the PXML,
  // so that they can have their own .desktop and icon without collisions, but still use the
  // same unique-id if they want to.
  //   To avoid breaking existing PXML's (even though we're pre-launch), can detect if ID
  // is present in PXML line or not; if not, assume application mode?
  hRoot = TiXmlHandle(pElem);

  if ( hRoot.FirstChild(PND_PXML_APP).Element() != NULL ) {
    appwrappermode = 1;
    appElem = hRoot.FirstChild(PND_PXML_APP).Element();
    hRoot = TiXmlHandle ( appElem );
  }

  // until we run out of applications in the PXML..
  while ( 1 ) {

    //pnd_log ( PND_LOG_DEFAULT, (char*)"  App #%u inside of PXML %s\n", appcount, pFilename );

    // create the buffer to hold the pxml
    apps [ appcount ] = (pnd_pxml_t*) malloc ( sizeof(pnd_pxml_t) );
    memset ( apps [ appcount ], '\0', sizeof(pnd_pxml_t) );

    // due to old code, just make life easier a minute..
    app = apps [ appcount ];
    if ( appwrappermode ) {
      app -> subapp_number = appcount;
    } else {
      app -> subapp_number = 0;
    }

    //Get unique ID first.
    if ( appwrappermode ) {
      app->unique_id = pnd_pxml_get_attribute(appElem, PND_PXML_ATTRNAME_UID);
      //pnd_log ( PND_LOG_DEFAULT, (char*)"  Subapp #%u has unique_id %s\n", appcount, app -> unique_id );
      app->appdata_dirname = pnd_pxml_get_attribute(appElem, PND_PXML_ATTRNAME_APPDATANAME);
    } else {
      app->unique_id = pnd_pxml_get_attribute(hRoot.Element(), PND_PXML_ATTRNAME_UID);
      //pnd_log ( PND_LOG_DEFAULT, (char*)"  Only-app #%u has unique_id %s\n", appcount, app -> unique_id );
    }

    //Everything related to the title:
    pnd_pxml_parse_titles(hRoot, app);

    //Everything description-related:
    pnd_pxml_parse_descriptions(hRoot, app);

    //Everything launcher-related in one tag:
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_EXEC).Element()) )
     {
       app->background = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECBG); //if this returns NULL, the struct is filled with NULL. No need to check.
       app->standalone = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECSTAL);
       app->exec       = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECCMD);
       app->startdir   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECWD);
       app->exec_no_x11     = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECNOX11);
       app->execargs   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECARGS);
     }

    //The app icon:
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_ICON).Element()) ) {
      app->icon       = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_ICONSRC);
    }

    // <info>
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_INFO).Element()) )
     {
       app-> info_name = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_INFONAME );
       app-> info_filename = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_INFOSRC );
       app-> info_type = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_INFOTYPE );
     }

    //The preview pics:
    if ( (pElem = hRoot.FirstChild(PND_PXML_NODENAME_PREVPICS).Element()) )
    {
      //TODO: Change this if pnd_pxml_t gains the feature of more pics than 2.
      if ( (pElem = pElem->FirstChildElement(PND_PXML_ENAME_PREVPIC)) )
      {
        app->previewpic1 = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PREVPICSRC);

        if ( (pElem = pElem->NextSiblingElement(PND_PXML_ENAME_PREVPIC)) )
        {
          app->previewpic2 = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PREVPICSRC);
        }
      }
    } //previewpic

    //The author info:
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_AUTHOR).Element()) )
    {
      app->author_name    = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHORNAME);
      app->author_website = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHORWWW);
      //TODO: Uncomment this if the author gets email support.
      //app->author_email = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHOREMAIL));
    }

    //The version info:
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_VERSION).Element()) )
    {
      app->version_major   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERMAJOR);
      app->version_minor   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERMINOR);
      app->version_release = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERREL);
      app->version_build   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERBUILD);
    }

    //The OS version info:
    if ( (pElem = hRoot.FirstChild(PND_PXML_ENAME_OSVERSION).Element()) )
    {
      app->osversion_major   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERMAJOR);
      app->osversion_minor   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERMINOR);
      app->osversion_release = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERREL);
      app->osversion_build   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERBUILD);
    }

    int i; //For now, we need to keep track of the index of categories.
    //Categories:
    if ( (pElem = hRoot.FirstChildElement(PND_PXML_NODENAME_CATS).Element()) ) //First, enter the "categories" node.
    {
      i = 0;

      //Goes through all the top-level categories and their sub-categories. i helps limit these to 2.
      for (pElem = pElem->FirstChildElement(PND_PXML_ENAME_CAT); pElem && i < 2;
           pElem = pElem->NextSiblingElement(PND_PXML_ENAME_CAT), i++)
      {
        //TODO: Fix pnd_pxml_t so that there can be more than 2 category 'trees' and more than 2 subcategories. Then this can be removed.
        switch (i)
        {
        case 0: //first category counts as the main cat for now
          app->main_category = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_CATNAME);
          break;

        case 1: //...second as the alternative
          app->altcategory = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_CATNAME);
        }

        TiXmlElement *pSubCatElem; //the sub-elements for a main category.
        int j = 0; //the subcategory index within this category

        //Goes through all the subcategories within this category. j helps limit these to 2.
        for (pSubCatElem = pElem->FirstChildElement(PND_PXML_ENAME_SUBCAT); pSubCatElem && j < 2;
             pSubCatElem = pSubCatElem->NextSiblingElement(PND_PXML_ENAME_SUBCAT), j++)
        {
          char *subcat = pnd_pxml_get_attribute(pSubCatElem, PND_PXML_ATTRNAME_SUBCATNAME);
          if (!(subcat)) continue;

          //TODO: This is ugly. Fix pnd_pxml_t so that there can be more than 2 category 'trees' and more than 2 subcategories. Then this can be removed.
          switch (j | (i << 1))
          {
          case 0:
            app->subcategory1 = subcat;
            break;
          case 1:
            app->subcategory2 = subcat;
            break;
          case 2:
            app->altsubcategory1 = subcat;
            break;
          case 3:
            app->altsubcategory2 = subcat;
          }
        }
      }
    }

    //All file associations:
    //Step into the associations node
    if ( (pElem = hRoot.FirstChild(PND_PXML_NODENAME_ASSOCS).Element()) )
    {
      i = 0;
      //Go through all associations. i serves as index; since the format only supports 3 associations we need to keep track of the number.
      for (pElem = pElem->FirstChildElement(PND_PXML_ENAME_ASSOC); pElem && i < 3;
           pElem = pElem->NextSiblingElement(PND_PXML_ENAME_ASSOC), i++)
      {
        char *name = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_ASSOCNAME);
        char *filetype = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_ASSOCFTYPE);
        char *paramter = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_ASSOCARGS);

        if (!(name && filetype && paramter)) continue;

        switch(i) //TODO: same problem here: only 3 associations supported
        {
        case 0:
        {
          app->associationitem1_name      = name;
          app->associationitem1_filetype  = filetype;
          app->associationitem1_parameter = paramter;
          break;
        }
        case 1:
        {
          app->associationitem2_name      = name;
          app->associationitem2_filetype  = filetype;
          app->associationitem2_parameter = paramter;
          break;
        }
        case 2:
        {
          app->associationitem3_name      = name;
          app->associationitem3_filetype  = filetype;
          app->associationitem3_parameter = paramter;
        }
        }
      }
    }

    //Performance related things (aka: Clockspeed XD):
    pElem = hRoot.FirstChild(PND_PXML_ENAME_CLOCK).Element();
    if (pElem)
    {
      app->clockspeed = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_CLOCKFREQ);
    }

    // Package
    pElem = hRoot.FirstChild ( PND_PXML_ENAME_PACKAGE ).Element();
    if ( pElem ) {
      app -> package_name = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_PACKAGE_NAME );
      app -> package_release_date = pnd_pxml_get_attribute ( pElem, PND_PXML_ATTRNAME_PACKAGE_DATE );
    }

    // mkdir request
    if ( (pElem = hRoot.FirstChild(PND_PXML_NODENAME_MKDIR).Element()) ) {

      // seek <dir>
      if ( (pElem = pElem->FirstChildElement(PND_PXML_ENAME_MKDIR)) ) {
        char *t;

        if ( ( t = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_MKDIRPATH) ) ) {
          // first <dir>, so just replace it wholesale; we use strdup so we can free() easily later, consistently. Mmm, leak seems imminent.
          app -> mkdir_sp = strdup ( t );
        }

        while ( ( pElem = pElem -> NextSiblingElement ( PND_PXML_ENAME_MKDIR ) ) ) {

          if ( ( t = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_MKDIRPATH) ) ) {
            char *foo = (char*) malloc ( strlen ( app -> mkdir_sp ) + strlen ( t ) + 1 /*:*/ + 1 /*\0*/ );

            if ( foo ) {
              sprintf ( foo, "%s:%s", app -> mkdir_sp, t );
              free ( app -> mkdir_sp );
              app -> mkdir_sp = foo;
            } // assuming we got ram, lets cat it all together

          } // got another elem?

        } // while

      } // found a <dir>

#if 0
      if ( app -> mkdir_sp ) {
        printf ( "mkdir: %s\n", app -> mkdir_sp );
      }
#endif

    } // mkdir

    // if in <application> mode, do we find another app?
    if ( appwrappermode ) {
      appElem = appElem -> NextSiblingElement ( PND_PXML_APP );
      if ( ! appElem ) {
        //pnd_log ( PND_LOG_DEFAULT, (char*)"  No more applications within PXML\n" );
        break; // no more applications
      }
      // got another application..
      //pnd_log ( PND_LOG_DEFAULT, "  Found another applications within PXML\n" );
      appwrappermode++;
      hRoot = TiXmlHandle ( appElem );

      appcount++;

      if ( appcount == PXML_MAXAPPS ) {
        return ( 1 ); // thats all we can handle; we're not going to auto-extend this
      }

    } else {
      break; // single-app old PXML
    }

  } // while finding apps

  return (1);
}

} // extern C

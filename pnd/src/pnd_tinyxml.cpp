
#include "tinyxml/tinyxml.h"
#include "../include/pnd_pxml.h"
#include "pnd_tinyxml.h"

//Easily change the tag names if required (globally in this file):
#include "pnd_pxml_names.h"

extern "C"
{

    unsigned char pnd_pxml_load(const char* pFilename, pnd_pxml_t *app)
    {
        FILE *f;
        char *b;
        unsigned int len;

        f = fopen(pFilename, "r");

        if (! f)
        {
            return (0);
        }

        fseek(f, 0, SEEK_END);

        len = ftell(f);

        fseek(f, 0, SEEK_SET);

        b = (char*) malloc(len);

        if (! b)
        {
            fclose(f);
            return (0);
        }

        fread(b, 1, len, f);

        fclose(f);

        return (pnd_pxml_parse(pFilename, b, len, app));
    }

    char *pnd_pxml_get_attribute(TiXmlElement *elem, const char *name)
    {
        const char *value = elem->Attribute(name);

        if (value)
            return strdup(value);
        else
            return NULL;
    }

    unsigned char pnd_pxml_parse_titles(const TiXmlHandle hRoot, pnd_pxml_t *app)
    {
        TiXmlElement *pElem;
        app->titles_alloc_c = 4;  //TODO: adjust this based on how many titles a PXML usually has. Power of 2.

        app->titles = (pnd_localized_string_t *)malloc(sizeof(pnd_localized_string_t) * app->titles_alloc_c);

        if (!app->titles) return (0); //errno = NOMEM

        //Go through all title tags and load them.
        for (pElem = hRoot.FirstChild(PND_PXML_ENAME_TITLE).Element(); pElem;
                pElem = pElem->NextSiblingElement(PND_PXML_ENAME_TITLE))
        {

            if (! pElem->GetText())
            {
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
        }

        return (1);
    }

    unsigned char pnd_pxml_parse_descriptions(const TiXmlHandle hRoot, pnd_pxml_t *app)
    {
        TiXmlElement *pElem;
        app->descriptions_alloc_c = 4; //TODO: adjust this based on how many descriptions a PXML usually has. Power of 2.

        app->descriptions = (pnd_localized_string_t *)malloc(sizeof(pnd_localized_string_t) * app->descriptions_alloc_c);

        if (!app->descriptions)
        {
            app->descriptions_alloc_c = 0;
            return (0); //errno = NOMEM
        }

        for (pElem = hRoot.FirstChild(PND_PXML_ENAME_DESCRIPTION).Element(); pElem;
                pElem = pElem->NextSiblingElement(PND_PXML_ENAME_DESCRIPTION))
        {

            if (! pElem->GetText())
            {
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
                app->descriptions = (pnd_localized_string_t*)realloc((void*)app->descriptions, app->descriptions_alloc_c);

                if (!app->descriptions) return (0); //errno = ENOMEM
            }

            pnd_localized_string_t *description = &app->descriptions[app->descriptions_c - 1];
            description->language = lang;
            description->string = text;
        }

        return (1);
    }

    unsigned char pnd_pxml_parse(const char *pFilename __attribute__((unused)), char *buffer, unsigned int length __attribute__((unused)), pnd_pxml_t *app)
    {
        //Load the XML document
        TiXmlDocument doc;
        doc.Parse(buffer);

        TiXmlElement *pElem = NULL;

        //Find the root element
        TiXmlHandle hDoc(&doc);
        TiXmlHandle hRoot(0);

        pElem = hDoc.FirstChild("PXML").Element();

        if (!pElem) return (0);

        hRoot = TiXmlHandle(pElem);

        //Get unique ID first.
        app->unique_id = pnd_pxml_get_attribute(hRoot.Element(), PND_PXML_ATTRNAME_UID);

        //Everything related to the title:
        pnd_pxml_parse_titles(hRoot, app);

        //Everything description-related:
        pnd_pxml_parse_descriptions(hRoot, app);

        //Everything launcher-related in one tag:
        if ((pElem = hRoot.FirstChild(PND_PXML_ENAME_EXEC).Element()))
        {
            app->background = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECBG); //if this returns NULL, the struct is filled with NULL. No need to check.
            app->standalone = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECSTAL);
            app->exec       = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECCMD);
            app->startdir   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECWD);
            app->exec_no_x11     = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_EXECNOX11);
        }

        //The app icon:
        if ((pElem = hRoot.FirstChild(PND_PXML_ENAME_ICON).Element()))
        {
            app->icon       = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_ICONSRC);
        }

        //The preview pics:
        if ((pElem = hRoot.FirstChild(PND_PXML_NODENAME_PREVPICS).Element()))
        {
            //TODO: Change this if pnd_pxml_t gains the feature of more pics than 2.
            if ((pElem = pElem->FirstChildElement(PND_PXML_ENAME_PREVPIC)))
            {
                app->previewpic1 = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PREVPICSRC);

                if ((pElem = pElem->NextSiblingElement(PND_PXML_ENAME_PREVPIC)))
                {
                    app->previewpic2 = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PREVPICSRC);
                }
            }
        } //previewpic

        //The author info:
        if ((pElem = hRoot.FirstChild(PND_PXML_ENAME_AUTHOR).Element()))
        {
            app->author_name    = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHORNAME);
            app->author_website = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHORWWW);
            //TODO: Uncomment this if the author gets email support.
            //app->author_email = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_AUTHOREMAIL));
        }

        //The version info:
        if ((pElem = hRoot.FirstChild(PND_PXML_ENAME_VERSION).Element()))
        {
            app->version_major   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERMAJOR);
            app->version_minor   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERMINOR);
            app->version_release = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERREL);
            app->version_build   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_VERBUILD);
        }

        //The OS version info:
        if ((pElem = hRoot.FirstChild(PND_PXML_ENAME_OSVERSION).Element()))
        {
            app->osversion_major   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERMAJOR);
            app->osversion_minor   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERMINOR);
            app->osversion_release = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERREL);
            app->osversion_build   = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_OSVERBUILD);
        }

        int i; //For now, we need to keep track of the index of categories.
        //Categories:
        if ((pElem = hRoot.FirstChildElement(PND_PXML_NODENAME_CATS).Element()))   //First, enter the "categories" node.
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
        if ((pElem = hRoot.FirstChild(PND_PXML_NODENAME_ASSOCS).Element()))
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

                switch (i) //TODO: same problem here: only 3 associations supported
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
        pElem = hRoot.FirstChild(PND_PXML_ENAME_PACKAGE).Element();

        if (pElem)
        {
            app -> package_name = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PACKAGE_NAME);
            app -> package_release_date = pnd_pxml_get_attribute(pElem, PND_PXML_ATTRNAME_PACKAGE_DATE);
        }

        return (1);
    }

} // extern C

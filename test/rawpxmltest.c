#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pnd_pxml.h>

//A simple test for the new format. No content-checking whatsoever...
//lots of ^C^V
//Used like a unit test, almost
int main (int argc, char **argv)
{
	if (argc != 2) 
	{
		printf("Wrong nr of args.\nUsage: %s <pxml filename>\n", argv[0]);
		return 1;
	}

	pnd_pxml_handle *apps = pnd_pxml_fetch(argv[1]);
	pnd_pxml_handle h;

	while ( *apps ) {
	  h = *apps;

	  if (!h)
	  {
	    printf("Could not load file \"%s\"\n", argv[1]);
	    return 1;
	  }

	  char *data; //for all values

	  if ( (data = pnd_pxml_get_app_name_en(h)) ) printf("Appname(en): %s\n", data);
	  if ( (data = pnd_pxml_get_app_name_de(h)) ) printf("Appname(de): %s\n", data);
	  if ( (data = pnd_pxml_get_app_name_it(h)) ) printf("Appname(it): %s\n", data);
	  if ( (data = pnd_pxml_get_app_name_fr(h)) ) printf("Appname(fr): %s\n", data);

	  if ( (data = pnd_pxml_get_unique_id(h)) ) printf("UID: %s\n", data);
     
     if ( (data = pnd_pxml_get_package_id(h)) ) printf("PID: %s\n", data);

	  if ( (data = pnd_pxml_get_standalone(h)) ) printf("Standalone: %s\n", data);

	  if ( (data = pnd_pxml_get_icon(h)) ) printf("Icon: %s\n", data);

	  if ( (data = pnd_pxml_get_description_en(h)) ) printf("Description(en): %s\n", data);
	  if ( (data = pnd_pxml_get_description_de(h)) ) printf("Description(de): %s\n", data);
	  if ( (data = pnd_pxml_get_description_it(h)) ) printf("Description(it): %s\n", data);
	  if ( (data = pnd_pxml_get_description_fr(h)) ) printf("Description(fr): %s\n", data);

	  if ( (data = pnd_pxml_get_previewpic1(h)) ) printf("Pic1: %s\n", data);
	  if ( (data = pnd_pxml_get_previewpic2(h)) ) printf("Pic2: %s\n", data);

	  if ( (data = pnd_pxml_get_author_name(h)) ) printf("Author name: %s\n", data);
	  if ( (data = pnd_pxml_get_author_website(h)) ) printf("Author website: %s\n", data);

	  if ( (data = pnd_pxml_get_version_major(h)) ) printf("Version major: %s\n", data);
	  if ( (data = pnd_pxml_get_version_minor(h)) ) printf("Version minor: %s\n", data);
	  if ( (data = pnd_pxml_get_version_release(h)) ) printf("Version release: %s\n", data);
	  if ( (data = pnd_pxml_get_version_build(h)) ) printf("Version build: %s\n", data);

	  if ( (data = pnd_pxml_get_osversion_major(h)) ) printf("OSVersion major: %s\n", data);
	  if ( (data = pnd_pxml_get_osversion_minor(h)) ) printf("OSVersion minor: %s\n", data);
	  if ( (data = pnd_pxml_get_osversion_release(h)) ) printf("OSVersion release: %s\n", data);
	  if ( (data = pnd_pxml_get_osversion_build(h)) ) printf("OSVersion build: %s\n", data);

	  if ( (data = pnd_pxml_get_exec(h)) ) printf("Application exec: %s\n", data);

	  if ( (data = pnd_pxml_get_main_category(h)) ) printf("Category 1: %s\n", data);
	  if ( (data = pnd_pxml_get_subcategory1(h)) ) printf("Category 1 sub 1: %s\n", data);
	  if ( (data = pnd_pxml_get_subcategory2(h)) ) printf("Category 1 sub 2: %s\n", data);
	  if ( (data = pnd_pxml_get_altcategory(h)) ) printf("Category 2: %s\n", data);
	  if ( (data = pnd_pxml_get_altsubcategory1(h)) ) printf("Category 2 sub 1: %s\n", data);
	  if ( (data = pnd_pxml_get_altsubcategory2(h)) ) printf("Category 2 sub 2: %s\n", data);

	  pnd_pxml_delete(h);

	  // next
	  apps++;
	} // while

	return 0;
}

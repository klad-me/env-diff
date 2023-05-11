#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>


#ifndef countof
	#define countof(x)	(sizeof(x) / sizeof((x)[0]))
#endif


typedef struct envVar_s
{
	const char *name;
	const char *value;
	
	struct envVar_s *next;
} envVar_t;


typedef struct path_s
{
	const char *value;
	
	struct path_s *next;
} path_t;


static const char* const skipVars[] =
{
	"_",
	"PWD",
	"OLDPWD",
};


static int usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-s] prev-env.txt\n", argv0);
	fprintf(stderr, "  Gets difference between previous (generated by `env` command) and current\n");
	fprintf(stderr, "  environment variables and prints it to stdout.\n");
	fprintf(stderr, "  Options:\n");
	fprintf(stderr, "    -s / --script  Make bash-script instead of VAR=VALUE entries\n");
	fprintf(stderr, "    -h / --help    This help\n");
	fprintf(stderr, "  Script options:\n");
	fprintf(stderr, "    -p / --patch   Try to patch $PATH instead of setting value\n");
	fprintf(stderr, "    -b / --begin   Place new entries of $PATH at the beginning (default is end)\n");
	return -1;
}


static envVar_t* parseEnv(const char *buf)
{
	const char *nameAt=buf;
	
	// Skip to '='
	while ( (*buf) && (*buf != '=') ) buf++;
	if (*buf != '=') return NULL;
	
	// Creating name
	const char *name=strndup(nameAt, buf-nameAt);
	
	// Checking blacklist
	for (int i=0; i<countof(skipVars); i++)
	{
		if (! strcmp(name, skipVars[i]))
		{
			free((void*)name);
			return NULL;
		}
	}
	
	// Creating value
	buf++;
	const char *valueAt=buf;
	while ( (*buf) && (*buf != '\r') && (*buf != '\n') ) buf++;
	const char *value=strndup(valueAt, buf-valueAt);
	
	// Creating variable
	envVar_t *var = (envVar_t*)malloc(sizeof(envVar_t));
	var->name=name;
	var->value=value;
	var->next=0;
	
	return var;
}


static path_t* parsePath(const char *s)
{
	path_t *path=0, **end=&path;
	
	while (*s)
	{
		const char *value=s;
		const char *valueEnd=strchr(s, ':');
		(*end)=(path_t*)malloc(sizeof(path_t));
		(*end)->next=0;
		if (valueEnd)
		{
			(*end)->value=strndup(value, valueEnd-value);
			end=&((*end)->next);
			s=valueEnd+1;
		} else
		{
			(*end)->value=strdup(value);
			break;
		}
	}
	
	return path;
}


static void printEscaped(const char *s)
{
	for ( ; *s; s++)
	{
		if (*s == '\"') putchar('\\');
		putchar(*s);
	}
}


int main(int argc, char **argv, char **env)
{
	bool script=false, patch=false, begin=false;
	char buf[65536];
	
	// Parsing command line options
	struct option opts[]=
	{
		{ "script",	no_argument,		0,	's' },
		{ "patch",	no_argument,		0,	'p' },
		{ "begin",	no_argument,		0,	'b' },
		{ "help",	no_argument,		0,	'h' },
		{ 0 }
	};
	int opt;
	while ( (opt=getopt_long(argc, argv, "spbh", opts, 0)) > 0)
	{
		switch (opt)
		{
			case 's':
				script=true;
				break;
			
			case 'p':
				patch=true;
				break;
			
			case 'b':
				begin=true;
				break;
			
			case 'h':
			default:
				return usage(argv[0]);
		}
	}
	if (optind+1 != argc) return usage(argv[0]);
	
	// Loading previous environment
	envVar_t *prevEnv=0, **end=&prevEnv;
	const char *inName=argv[optind];
	FILE *in=fopen(inName, "r");
	if (! in)
	{
		perror(inName);
		return -1;
	}
	while (fgets(buf, sizeof(buf), in))
	{
		envVar_t *var=parseEnv(buf);
		if (! var) continue;
		(*end)=var;
		end=&var->next;
	}
	fclose(in);
	
	// Diffing with current environment
	for ( ; *env; env++)
	{
		if (strlen(*env) >= sizeof(buf)) continue;
		
		envVar_t *var=parseEnv(*env);
		if (! var) continue;
		
		// Looking for previous value
		envVar_t *prev;
		for (prev=prevEnv; prev && (strcmp(var->name, prev->name) != 0); prev=prev->next);
		
		if ( (! prev) ||
			 (strcmp(var->value, prev->value) != 0) )
		{
			// New/updated variable
			if (script)
			{
				// Script
				if ( (patch) && (! strcmp(var->name, "PATH")) )
				{
					// Make path diff
					path_t *prevPath=parsePath(prev->value);
					path_t *path=parsePath(var->value);
					char *result=buf;
					for ( ; path; path=path->next)
					{
						path_t *v;
						for (v=prevPath; v; v=v->next)
							if (! strcmp(v->value, path->value))
								break;
						
						if (! v)
						{
							if (! begin) result=stpcpy(result, ":");
							result=stpcpy(result, path->value);
							if (begin) result=stpcpy(result, ":");
						}
					}
					*result=0;
					printf("export PATH=\"");
					if (! begin) printf("$PATH");
					printEscaped(buf);
					if (begin) printf("$PATH");
					printf("\"\n");
				} else
				{
					// Regular variable
					printf("export %s=\"", var->name);
					printEscaped(var->value);
					printf("\"\n");
				}
			} else
			{
				// KEY=VALUE
				printf("%s=%s\n", var->name, var->value);
			}
		}
	}
	
	// Done
	return 0;
}

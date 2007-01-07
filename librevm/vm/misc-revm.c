/*
** misc.c for elfsh
**
** Started on  Fri Nov  2 15:21:56 2001 mayhem
** Updated on  Fri Sep 11 17:26:11 2005 mayhem
*/
#include "revm.h"

#if defined(USE_READLN)
 rl_command_func_t *rl_ctrll = NULL;
#endif

/* return the right prompt */
char	*vm_get_prompt()
{
  if (world.state.vm_mode == ELFSH_VMSTATE_IMODE ||
      world.state.vm_mode == ELFSH_VMSTATE_DEBUGGER)
    {
      snprintf(prompt_token, sizeof(prompt_token),
	       "%s%s%s%s%s%s%s%s%s%s%s ",
	       vm_colorget("%s", "pspecial", "("),
	       (world.state.vm_mode == ELFSH_VMSTATE_DEBUGGER ?
		vm_colorget("%s", "psname" , E2DBG_ARGV0)    :
		vm_colorget("%s", "psname" , ELFSH_SNAME)),
	       vm_colorget("%s", "pspecial", "-"),
	       vm_colorget("%s", "pversion", ELFSH_VERSION),
	       vm_colorget("%s", "pspecial", "-"),
	       vm_colorget("%s", "prelease", ELFSH_RELEASE),
	       vm_colorget("%s", "pspecial", "-"),
	       vm_colorget("%s", "pedition", ELFSH_EDITION),
	       vm_colorget("%s", "pspecial", "@"),
	       vm_colorget("%s", "psname", world.curjob->name),
	       vm_colorget("%s", "pspecial", ")"));
      vm_endline();

#if defined(USE_READLN)
      /* Prompt on readline need some modifications */
      vm_rl_update_prompt(prompt_token, sizeof(prompt_token));
#endif

      return prompt_token;
    }

  if (world.state.vm_mode == ELFSH_VMSTATE_SCRIPT)
    return "";

  return "UNKNOWN MODE> ";
}

/* return the project name accordingly to mode */
char		*vm_get_mode_name()
{
  char          *mode;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  if (world.state.vm_mode == ELFSH_VMSTATE_DEBUGGER)
    mode = E2DBG_NAME;
  else
    mode = ELFSH_NAME;

  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (mode));
}


/* Our system implementation */
int		vm_system(char *cmd)
{
  char		buf[BUFSIZ];
  int		ret;
  char		**av;
  int		nbr;
  int		argc;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  if (world.curjob->io.type == ELFSH_IONET)
    snprintf(buf, BUFSIZ, "%s <&%u >&0 2>&0 ", cmd, world.curjob->io.output_fd);
  else
    snprintf(buf, BUFSIZ, "%s ", cmd);

  /* If the user shell is unspecified we use system */
  nbr = vm_findblanks(cmd);
  av = vm_doargv(nbr, (u_int *)&argc, cmd);
  av++;
  if (!fork())
    ret = execvp(av[0], av);
  else
    wait(NULL);

  /* Report result */
  if (ret < 0)
    ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__,
		      "Shell not found", 0);
  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (0));
}


/* Decide what to do for exiting depending on the current input */
void	vm_exit(int err)
{
  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);
  _exit(err);
  ELFSH_PROFILE_OUT(__FILE__, __FUNCTION__, __LINE__);
}

/* Bad parameter handler */
void	vm_badparam(char *str)
{
  char	buf[BUFSIZ];

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  snprintf(buf, BUFSIZ,
	   "\n [!] Invalid parameters for command %s .::. "
	   "type 'help' for command list \n\n", str);
  vm_output(buf);

  ELFSH_PROFILE_OUT(__FILE__, __FUNCTION__, __LINE__);
}

/* Unknow command handler */
void	vm_unknown(char *str)
{
  char	buf[BUFSIZ];

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  snprintf(buf, BUFSIZ, "\n [!] Unknown command %s .::. "
	   "type 'help' for command list \n\n", str);
  vm_output(buf);
  ELFSH_PROFILE_OUT(__FILE__, __FUNCTION__, __LINE__);
}

/* Generic error message handler */

void	vm_error(char *label, char *param)
{
  char	buf[BUFSIZ];

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  snprintf(buf, BUFSIZ, " [!] %s [%s] \n\n", label, param);
  vm_output(buf);
  ELFSH_PROFILE_OUT(__FILE__, __FUNCTION__, __LINE__);
}


/* Open the script file */
int		vm_openscript(char **av)
{
  int		fd;
  int		idx;
  char		actual[16];
  revmobj_t	*new;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  /* Open script file */
  XOPEN(fd, av[0], O_RDONLY, 0, -1);

  world.curjob->io.input_fd = fd;

  /* Add argument variables */
  for (idx = 1; av[idx]; idx++)
    {
      snprintf(actual, sizeof(actual), "%u", idx);
      new = vm_create_IMMEDSTR(1, elfsh_strdup(av[idx]));
      hash_add(&vars_hash, elfsh_strdup(actual), new);
    }

  new = vm_create_IMMED(ELFSH_OBJINT, 1, idx);
  hash_add(&vars_hash, ELFSH_ARGVAR, new);

  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, 0);
}

/* Say if we are in script mode */
int		vm_testscript(int ac, char **av)
{
  int		fd;
  char		buff[30];

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  if (ac < 2 || (av[1] && av[1][0] == ELFSH_MINUS))
    ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, 0);
  XOPEN(fd, av[1], O_RDONLY, 0, 0);
  XREAD(fd, buff, 30, 0);
  buff[29] = 0x00;
  if (strncmp(buff, "#!", 2))
    {
      XCLOSE(fd, 0);
      ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__, "Invalid script interpreter", 0);
    }
  if (!strstr(buff, av[0]) && !strstr(buff, "elfsh"))
    {
      XCLOSE(fd, 0);
      ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__, "Not an Elfsh script", 0);
    }
  XCLOSE(fd, 0);
  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (1));
}

/* Print the banner */
void		vm_print_banner()
{
  char		logbuf[BUFSIZ];

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  snprintf(logbuf, BUFSIZ - 1,
	   "\n\n\t The %s %s (%s) .::. \n\n %s",
	   vm_get_mode_name(),
	   ELFSH_VERSION,
#if defined(ELFSH32)
	   "32 bits built",
#elif defined(ELFSH64)
	   "64 bits built",
#else
	   "Unknown built",
#endif
	   "\t .::. This software is under the General Public License V.2 \n"
	   "\t .::. Please visit http://www.gnu.org \n\n");
  vm_output(logbuf);
  ELFSH_PROFILE_OUT(__FILE__, __FUNCTION__, __LINE__);
}

/* Print the Unknown buffer */
char		*vm_build_unknown(char *buf, const char *str, u_long type)
{

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  snprintf(buf, ELFSH_MEANING, "%s %08X", str, (u_int) type);

  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (buf));
}


/* Retreive a file object giving its unique ID */
elfshobj_t	*vm_getfile(u_int id)
{
  elfshobj_t	*cur;
  elfshobj_t	*subcur;
  char		**keys;
  int		idx;
  int		keynbr;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  /* Check in private objects of the workspace */
  if (hash_size(&world.curjob->loaded))
    {
      keys = hash_get_keys(&world.curjob->loaded, &keynbr);
      for (idx = 0; idx < keynbr; idx++)
	{
	  cur = hash_get(&world.curjob->loaded, keys[idx]);
	  if (cur->id == id)
	    ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (cur));

	  if ((subcur = vm_is_depid(cur, id)) != NULL)
	    ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (subcur));
	}
    }

  /* Check in shared objects */
  if (hash_size(&world.shared_hash))
    {
      keys = hash_get_keys(&world.shared_hash, &keynbr);
      for (idx = 0; idx < keynbr; idx++)
	{
	  cur = hash_get(&world.shared_hash, keys[idx]);
	  if (cur->id == id)
	    ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (cur));

	  if ((subcur = vm_is_depid(cur, id)) != NULL)
	    ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (subcur));
	}
    }

  ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__,
		    "Unable to find file", (NULL));
}

/* Retreive a module object giving its unique ID */
revmmod_t	*vm_getmod(u_int index)
{
  revmmod_t	*cur;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  for (cur = world.modlist; cur; cur = cur->next)
    if (cur->id == index)
      ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (cur));
  ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__,
		    "Unable to find module", (NULL));
}

/* Print error depending on the state of the machine */
int		vm_doerror(void (*fct)(char *str), char *str)
{
  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  if (world.state.vm_mode != ELFSH_VMSTATE_CMDLINE || world.state.vm_net)
    fct(str);
  else
    {
      cmd_help();
      exit(-1);
    }
  ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__,
		    "Bypassed error printing", (-1));
}


/* Change the shell variable */
int		vm_setshell(char *str)
{
  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);
  if (vm_setvar_str(ELFSH_SHELLVAR, str) < 0)
   ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__,
		     "Cannot modify shell var", -1);
  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, 0);
}


/* The internal basename function */
char		*vm_basename(char *str)
{
  char		*cur;
  char		*ret;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  ret = NULL;
  cur = str;
  while ((cur = strchr(cur, '/')))
    if (!*(cur + 1))
      ELFSH_PROFILE_ERR(__FILE__, __FUNCTION__, __LINE__, "No basename", (NULL));
    else
      ret = ++cur;
  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (ret));
}



/* Useful to differentiate 0 and a string */
int	vm_isnbr(char *string)
{
  size_t len = strlen(string);
  size_t ii;

  ELFSH_PROFILE_IN(__FILE__, __FUNCTION__, __LINE__);

  for (ii=0; ii < len; ii++)
    if (!isdigit((int) string[ii]))
      ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, 0);
  ELFSH_PROFILE_ROUT(__FILE__, __FUNCTION__, __LINE__, (1));
}
// source/idk/main.c

puts(const char *);

main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	(void) argc;
	(void) argv;
	(void) envp;

	puts("\nhello, world");

	return 0;
}

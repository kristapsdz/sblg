#ifndef EXTERN_H
#define EXTERN_H

__BEGIN_DECLS

struct	article {
	const char	*src;
	char		*title;
	size_t		 titlesz;
	time_t	 	 time;
};

#define	xrealloc realloc
#define	xcalloc	calloc
#define	xmalloc	malloc
#define	xstrdup	strdup

int	grok(XML_Parser p, const char *src,
		struct article *data);
int	compile(XML_Parser p, const char *templ,
		const char *src, const char *dst);
void	mmap_close(int fd, void *buf, size_t sz);
int	mmap_open(const char *f, int *fd, char **buf, size_t *sz);
int	linkall(XML_Parser p, const char *templ,
		int sz, char *src[], const char *dst);
void	xmlprint(FILE *f, const XML_Char *s, const XML_Char **atts);
int	xmlvoid(const XML_Char *s);

__END_DECLS

#endif 

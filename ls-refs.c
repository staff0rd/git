#include "cache.h"
#include "repository.h"
#include "refs.h"
#include "remote.h"
#include "argv-array.h"
#include "ls-refs.h"
#include "pkt-line.h"

struct ref_pattern {
	char *pattern;
	int wildcard_pos; /* If > 0, indicates the position of the wildcard */
};

struct pattern_list {
	struct ref_pattern *patterns;
	int nr;
	int alloc;
};

static void add_pattern(struct pattern_list *patterns, const char *pattern)
{
	struct ref_pattern p;
	const char *wildcard;

	p.pattern = strdup(pattern);

	wildcard = strchr(pattern, '*');
	if (wildcard) {
		p.wildcard_pos = wildcard - pattern;
	} else {
		p.wildcard_pos = -1;
	}

	ALLOC_GROW(patterns->patterns,
		   patterns->nr + 1,
		   patterns->alloc);
	patterns->patterns[patterns->nr++] = p;
}

static void clear_patterns(struct pattern_list *patterns)
{
	int i;
	for (i = 0; i < patterns->nr; i++)
		free(patterns->patterns[i].pattern);
	FREE_AND_NULL(patterns->patterns);
	patterns->nr = 0;
	patterns->alloc = 0;
}

/*
 * Check if one of the patterns matches the tail part of the ref.
 * If no patterns were provided, all refs match.
 */
static int ref_match(const struct pattern_list *patterns, const char *refname)
{
	int i;

	if (!patterns->nr)
		return 1; /* no restriction */

	for (i = 0; i < patterns->nr; i++) {
		const struct ref_pattern *p = &patterns->patterns[i];

		/* No wildcard, exact match expected */
		if (p->wildcard_pos < 0) {
			if (!strcmp(refname, p->pattern))
				return 1;
		} else {
			/* Wildcard, prefix match until the wildcard */
			if (!strncmp(refname, p->pattern, p->wildcard_pos))
				return 1;
		}
	}

	return 0;
}

struct ls_refs_data {
	unsigned peel;
	unsigned symrefs;
	struct pattern_list patterns;
};

static int send_ref(const char *refname, const struct object_id *oid,
		    int flag, void *cb_data)
{
	struct ls_refs_data *data = cb_data;
	const char *refname_nons = strip_namespace(refname);
	struct strbuf refline = STRBUF_INIT;

	if (!ref_match(&data->patterns, refname))
		return 0;

	strbuf_addf(&refline, "%s %s", oid_to_hex(oid), refname_nons);
	if (data->symrefs && flag & REF_ISSYMREF) {
		struct object_id unused;
		const char *symref_target = resolve_ref_unsafe(refname, 0,
							       &unused,
							       &flag);

		if (!symref_target)
			die("'%s' is a symref but it is not?", refname);

		strbuf_addf(&refline, " symref-target:%s", symref_target);
	}

	if (data->peel) {
		struct object_id peeled;
		if (!peel_ref(refname, &peeled))
			strbuf_addf(&refline, " peeled:%s", oid_to_hex(&peeled));
	}

	strbuf_addch(&refline, '\n');
	packet_write(1, refline.buf, refline.len);

	strbuf_release(&refline);
	return 0;
}

int ls_refs(struct repository *r, struct argv_array *keys, struct argv_array *args)
{
	int i;
	struct ls_refs_data data;

	memset(&data, 0, sizeof(data));

	for (i = 0; i < args->argc; i++) {
		const char *arg = args->argv[i];
		const char *out;

		if (!strcmp("peel", arg))
			data.peel = 1;
		else if (!strcmp("symrefs", arg))
			data.symrefs = 1;
		else if (skip_prefix(arg, "ref-pattern ", &out))
			add_pattern(&data.patterns, out);
	}

	head_ref_namespaced(send_ref, &data);
	for_each_namespaced_ref(send_ref, &data);
	packet_flush(1);
	clear_patterns(&data.patterns);
	return 0;
}

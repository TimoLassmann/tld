/* template: fill {{name}} placeholders and {{IF name}}...{{ELSE}}...{{ENDIF}} blocks in
 * text from a tld_hash of names to values. */
#ifndef TLD_TEMPLATE_H
#define TLD_TEMPLATE_H

#include "../core/tld-core.h"

#include "../string/tld-string.h"
typedef struct tld_hash tld_hash;

/* Render the template in txt in place. {{name}} becomes map's value for name (nothing
 * when absent); {{IF name}}A{{ELSE}}B{{ENDIF}} keeps A when name has a non-empty value
 * and B otherwise (ELSE is optional; IFs nest up to 512 deep). The text between the
 * braces is the key as written (no trimming, 1 to 63 characters). map must not be NULL.
 * A malformed template (unclosed IF or tag, ELSE or ENDIF without IF, an empty {{}}, an
 * IF with no name, IFs nested deeper than 512) prints the reason with its byte offset
 * and returns FAIL with txt unchanged.
 *
 *   tld_hash *m = NULL;
 *   RUN(tld_hash_add(&m, "who", "world"));
 *   RUN(tld_strbuf_append(txt, "Hello {{who}}{{IF excl}}!{{ENDIF}}"));
 *   RUN(tld_template_apply(txt, m));        // txt is now "Hello world"
 */
TLD_API int tld_template_apply(tld_strbuf *txt, tld_hash *map);

/* A new map in *hash (free with tld_hash_free) whose keys are the variable names used
 * in txt, from {{name}} and {{IF name}}, each mapped to itself. */
TLD_API int tld_template_vars(tld_strbuf *txt, tld_hash **hash);
#endif

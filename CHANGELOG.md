# Changelog

## modernisation branch (unreleased)

Interface promise: public function names and signatures, struct layouts and macro names are
unchanged, except that the seq/ and hdf5/ modules were removed. Behaviour changes listed
here fix bugs.

### Removed
- seq/ (FASTA/FASTQ/SAM IO, alphabets, reverse complement) and hdf5/. tld now depends only on
  libc, libm and pthreads.

### Changed
- Licence: MIT (was BSD-style).

# Tests added by the tpool fixes (modernisation M1/M2). Use add_custom_test(name file).
# unit-tpool is built in CMakeLists.txt; register it with ctest here.
add_test(NAME unit-tpool COMMAND unit-tpool)
add_custom_test(unit-fix-rng utests/unit_fix_rng.c)
add_custom_test(unit-fix-kdtree utests/unit_fix_kdtree.c)

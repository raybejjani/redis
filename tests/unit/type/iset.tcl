start_server {tags {"iset"}} {
    test "ISET basic IADD" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 12 22 y
        r iadd itmp 15 25 z
        assert_equal_elements {x y z} [r istab itmp 18]
        assert_equal_elements {y z} [r istab itmp 21]
    }

    test "ISET Boundaries" {
        r del itmp
        r iadd itmp 10 20 x
        assert_equal {x} [r istab itmp 10]
        assert_equal {x} [r istab itmp 20]
    }

    test "ISET floating point stab" {
        r del itmp
        r iadd itmp 1 1.5 x
        r iadd itmp 1.25 2 y
        assert_equal {} [r istab itmp 0.99999]
        assert_equal {x} [r istab itmp 1.2]
        assert_equal_elements {x y} [r istab itmp 1.25]
        assert_equal {} [r istab itmp 2.1111]
    }

    test "ISET score update" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 15 25 x
        assert_equal {x} [r istab itmp 24]
        assert_equal {} [r istab itmp 14]
    }
}

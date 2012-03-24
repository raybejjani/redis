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
}

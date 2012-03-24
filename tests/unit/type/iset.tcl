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

    test "ISET wrong number of arguments" {
        assert_error "*wrong*number*arguments*" {r iadd itmp 2}
        assert_error "*wrong*number*arguments*" {r iadd itmp 2 3}
        assert_error "*wrong*number*arguments*" {r iadd itmp 2 3 too 2}
        assert_error "*wrong*number*arguments*" {r iadd itmp 2 3 too 2 3}
        assert_error "*wrong*number*arguments*" {r iadd itmp 2 3 too 2 3 many more}
    }

    test "ISET Can't update a key without floats" {
        assert_error "*not*float*" {r iadd itmp banana cream pie}
        assert_error "*not*float*" {r iadd itmp 2 cream pies}
    }

    test "ISET variadic IADD" {
        r del itmp
        r iadd itmp 10 20 x 12 22 y 15 25 z
        assert_equal_elements {x y z} [r istab itmp 18]
        assert_equal_elements {y z} [r istab itmp 21]
    }

    test "ISET IADD returns correct value" {
        r del itmp
        r iadd itmp 10 20 x
    } {1}

    test "ISET IADD returns correct zero" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 10 20 x
    } {0}

    test "ISET variadic IADD returns correct value" {
        r del itmp
        r iadd itmp 10 20 x 12 22 y 15 25 z
    } {3}

    test "ISET IADD returns correct value after insert" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 12 22 y
        r iadd itmp 10 20 x 12 22 y 15 25 z
    } {1}
}

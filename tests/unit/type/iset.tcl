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
        assert_equal {1} [r iadd itmp 10 20 x]
    }

    test "ISET IADD returns correct zero" {
        r del itmp
        r iadd itmp 10 20 x
        assert_equal {0} [r iadd itmp 10 20 x]
    }

    test "ISET variadic IADD returns correct value" {
        r del itmp
        assert_equal {3} [r iadd itmp 10 20 x 12 22 y 15 25 z]
    }

    test "ISET IADD returns correct value after insert" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 12 22 y
        assert_equal {1} [r iadd itmp 10 20 x 12 22 y 15 25 z]
    }

    test "ISET ISTAB basics" {
        r del itmp
        r iadd itmp 1 10 a
        r iadd itmp 3 10 b
        r iadd itmp 5 10 c
        r iadd itmp 7 12 d

        assert_equal_elements {a b c d} [r istab itmp 8]
        assert_equal_elements {a b c} [r istab itmp 6]
        assert_equal_elements {a b} [r istab itmp 4]
        assert_equal_elements {a} [r istab itmp 2]
        assert_equal_elements {d} [r istab itmp 11]
        assert_equal_elements {} [r istab itmp 0]
        assert_equal_elements {} [r istab itmp 13]

        #this doesn't really work, because assert_equal_elements
        #will verify {1 2 a 3 4 b} and {3 4 a 1 2 b} as the same set.
        #TODO: figure out a better assert
        #assert_equal_elements {}
    }

    test "ISET ISTAB weird scenario I hit that caused a bug" {
        r del itmp
        r iadd itmp 1 10 x
        assert_error "*not*valid*float*" {r istab x withintervals}
        assert_equal {x} [r istab itmp 5]
    }

    test "ISET adding multiple keys to a node" {
        r del itmp
        r iadd itmp 1 3 x
        r iadd itmp 1 3 y
        assert_equal {x y} [r istab itmp 2]
    }

    test "ISET IREM basics" {
        r del itmp
        r iadd itmp 1 3 x
        assert_equal {1} [r irem itmp x]
        assert_error "*no*such*key" {r istab itmp 2}
    }

    test "ISET adding multiple keys to a node then deleting one" {
        r del itmp
        r iadd itmp 1 3 x
        r iadd itmp 1 3 y
        r irem itmp x
        assert_equal {y} [r istab itmp 2]
    }

    test "IREMBYSTAB empty" {
        r del itmp
        r iadd itmp 0 1 x
        assert_equal {0} [r irembystab itmp 2]
    }

    test "IREMBYSTAB basics" {
        r del itmp
        r iadd itmp 1 4 x
        r iadd itmp 2 5 y
        r iadd itmp 20 50 z
        r irembystab itmp 2
        assert_equal {} [r istab itmp 2]
        assert_equal {z} [r istab itmp 25]
    }
}
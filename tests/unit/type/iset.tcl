start_server {tags {"iset"}} {
    test "ISET basic IADD" {
        r del itmp
        r iadd itmp 10 20 x
        r iadd itmp 12 22 y
        r iadd itmp 15 25 z
        assert_equal {x y z} [r istab itmp 18]
    }
}

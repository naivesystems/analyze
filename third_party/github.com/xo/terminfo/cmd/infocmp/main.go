// Application infocmp should have the same output as the standard Unix infocmp
// -1 -L output.
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"sort"
	"strings"
	"unicode"

	"github.com/xo/terminfo"
)

var (
	flagTerm     = flag.String("term", os.Getenv("TERM"), "term name")
	flagExtended = flag.Bool("x", false, "extended options")
)

func main() {
	flag.Parse()

	ti, err := terminfo.Load(*flagTerm)
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("#\tReconstructed via %s from file: %s\n", strings.TrimPrefix(os.Args[0], "./"), ti.File)
	fmt.Printf("%s,\n", strings.TrimSpace(strings.Join(ti.Names, "|")))

	process(ti.BoolCaps, ti.ExtBoolCaps, ti.BoolsM, terminfo.BoolCapName, nil)
	process(
		ti.NumCaps, ti.ExtNumCaps, ti.NumsM, terminfo.NumCapName,
		func(v interface{}) string { return fmt.Sprintf("#%d", v) },
	)
	process(
		ti.StringCaps, ti.ExtStringCaps, ti.StringsM, terminfo.StringCapName,
		func(v interface{}) string { return "=" + escape(v.([]byte)) },
	)
}

func process(x, y interface{}, m map[int]bool, name func(int) string, mask func(interface{}) string) {
	printIt(x, m, name, mask)
	if *flagExtended {
		printIt(y, nil, name, mask)
	}
}

// process walks the values in z, adding missing elements in m. a mask func can
// be provided to format the values in z.
func printIt(z interface{}, m map[int]bool, name func(int) string, mask func(interface{}) string) {
	var names []string
	x := make(map[string]string)
	switch v := z.(type) {
	case func() map[string]bool:
		for n, a := range v() {
			if !a {
				continue
			}
			var f string
			if mask != nil {
				f = mask(a)
			}
			x[n], names = f, append(names, n)
		}

	case func() map[string]int:
		for n, a := range v() {
			if a < 0 {
				continue
			}
			var f string
			if mask != nil {
				f = mask(a)
			}
			x[n], names = f, append(names, n)
		}

	case func() map[string][]byte:
		for n, a := range v() {
			if a == nil {
				continue
			}
			var f string
			if mask != nil {
				f = mask(a)
			}
			if n == "acs_chars" && strings.TrimSpace(strings.TrimPrefix(f, "=")) == "" {
				continue
			}
			x[n], names = f, append(names, n)
		}
	}

	// add missing
	for i := range m {
		n := name(i)
		x[n], names = "@", append(names, n)
	}

	// sort and print
	sort.Strings(names)
	for _, n := range names {
		fmt.Printf("\t%s%s,\n", n, x[n])
	}
}

// peek peeks a byte.
func peek(b []byte, pos, length int) byte {
	if pos < length {
		return b[pos]
	}
	return 0
}

func isprint(b byte) bool {
	return unicode.IsPrint(rune(b))
}

func realprint(b byte) bool {
	return b < 127 && isprint(b)
}

func iscntrl(b byte) bool {
	return unicode.IsControl(rune(b))
}

func realctl(b byte) bool {
	return b < 127 && iscntrl(b)
}

func isdigit(b byte) bool {
	return unicode.IsDigit(rune(b))
}

// logic taken from _nc_tic_expand from ncurses-6.0/ncurses/tinfo/comp_expand.c
func escape(buf []byte) string {
	length := len(buf)
	if length == 0 {
		return ""
	}

	var s []byte
	islong := length > 3
	for i := 0; i < length; i++ {
		ch := buf[i]
		switch {
		case ch == '%' && realprint(peek(buf, i+1, length)):
			s = append(s, buf[i], buf[i+1])
			i++

		case ch == 128:
			s = append(s, '\\', '0')

		case ch == '\033':
			s = append(s, '\\', 'E')

		case realprint(ch) && ch != ',' && ch != ':' && ch != '!' && ch != '^':
			s = append(s, ch)

		case ch == '\r' && (islong || (i == length-1 && length > 2)):
			s = append(s, '\\', 'r')

		case ch == '\n' && islong:
			s = append(s, '\\', 'n')

		case realctl(ch) && ch != '\\' && (!islong || isdigit(peek(buf, i+1, length))):
			s = append(s, '^', ch+'@')

		default:
			s = append(s, []byte(fmt.Sprintf("\\%03o", ch))...)
		}
	}

	return string(s)
}

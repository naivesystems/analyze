package terminfo

import (
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"reflect"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"testing"
)

func TestOpen(t *testing.T) {
	for term, filename := range terms(t) {
		t.Run(strings.TrimPrefix(filename, "/"), func(term, filename string) func(*testing.T) {
			return func(t *testing.T) {
				t.Parallel()

				// open
				ti, err := Open(filepath.Dir(filepath.Dir(filename)), term)
				if err != nil {
					t.Fatalf("term %s expected no error, got: %v", term, err)
				}

				if ti.File != filename {
					t.Errorf("term %s should have file %s, got: %s", term, filename, ti.File)
				}

				// check we have at least one name
				if len(ti.Names) < 1 {
					t.Errorf("term %s expected names to have at least one value", term)
				}
			}
		}(term, filename))
	}
}

var infocmpMap = struct {
	ic map[string]*infocmp
	sync.RWMutex
}{
	ic: make(map[string]*infocmp),
}

func TestValues(t *testing.T) {
	// load infocmp data
	err := loadInfocmpData(t)
	if err != nil {
		t.Fatal(err)
	}

	for term, filename := range terms(t) {
		t.Run(strings.TrimPrefix(filename, "/"), func(t *testing.T) {
			t.Parallel()

			infocmpMap.RLock()
			ic := infocmpMap.ic[term]
			infocmpMap.RUnlock()

			// load
			ti, err := Load(term)
			if err != nil {
				t.Fatalf("term %s expected no error, got: %v", term, err)
			}

			// check names
			if !reflect.DeepEqual(ic.names, ti.Names) {
				t.Errorf("term %s names do not match", term)
			}

			// check bool caps
			for i, v := range ic.boolCaps {
				if v == nil {
					if _, ok := ti.BoolsM[i]; !ok {
						t.Errorf("term %s expected bool cap %d (%s) to be missing", term, i, BoolCapName(i))
					}
				} else if v.(bool) != ti.Bools[i] {
					t.Errorf("term %s bool cap %d (%s) should be %t", term, i, BoolCapName(i), v)
				}
			}

			// check extended bool caps
			if len(ic.extBoolCaps) != len(ti.ExtBools) {
				t.Errorf("term %s should have same number of extended bools (%d, %d)", term, len(ic.extBoolCaps), len(ti.ExtBools))
			}
			for i, v := range ic.extBoolCaps {
				z, ok := ti.ExtBools[i]
				if !ok {
					t.Errorf("term %s should have extended bool %d", term, i)
				}
				if v.(bool) != z {
					t.Errorf("term %s extended bool cap %d (%s) should be %t", term, i, ic.extBoolNames[i], v)
				}

				n, ok := ti.ExtBoolNames[i]
				if !ok {
					t.Errorf("term %s missing extended bool %d name", term, i)
				}
				if string(n) != ic.extBoolNames[i] {
					t.Errorf("term %s extended bool %d name should be '%s', got: '%s'", term, i, ic.extBoolNames[i], string(n))
				}
			}

			// check num caps
			for i, v := range ic.numCaps {
				if v == nil {
					if _, ok := ti.NumsM[i]; !ok {
						//t.Errorf("term %s expected num cap %d (%s) to be missing", term, i, NumCapName(i))
					}
				} else if v.(int) != ti.Nums[i] {
					t.Errorf("term %s num cap %d (%s) should be %d", term, i, NumCapName(i), v)
				}
			}

			// check extended num caps
			if len(ic.extNumCaps) != len(ti.ExtNums) {
				t.Errorf("term %s should have same number of extended nums (%d, %d)", term, len(ic.extNumCaps), len(ti.ExtNums))
			}
			for i, v := range ic.extNumCaps {
				z, ok := ti.ExtNums[i]
				if !ok {
					t.Errorf("term %s should have extended num %d", term, i)
				}
				if v.(int) != z {
					t.Errorf("term %s extended num cap %d (%s) should be %t", term, i, ic.extNumNames[i], v)
				}

				n, ok := ti.ExtNumNames[i]
				if !ok {
					t.Errorf("term %s missing extended num %d name", term, i)
				}
				if string(n) != ic.extNumNames[i] {
					t.Errorf("term %s extended num %d name should be '%s', got: '%s'", term, i, ic.extNumNames[i], string(n))
				}
			}

			// check string caps
			for i, v := range ic.stringCaps {
				if v == nil {
					if _, ok := ti.StringsM[i]; !ok {
						//t.Errorf("term %s expected string cap %d (%s) to be missing", term, i, StringCapName(i))
					}
				} else if v.(string) != string(ti.Strings[i]) {
					t.Errorf("term %s string cap %d (%s) is invalid:", term, i, StringCapName(i))
					t.Errorf("got:  %#v", string(ti.Strings[i]))
					t.Errorf("want: %#v", v)
				}
			}

			// check extended string caps
			if len(ic.extStringCaps) != len(ti.ExtStrings) {
				t.Errorf("term %s should have same number of extended strings (%d, %d)", term, len(ic.extStringCaps), len(ti.ExtStrings))
			}
			for i, v := range ic.extStringCaps {
				z, ok := ti.ExtStrings[i]
				if !ok {
					t.Errorf("term %s should have extended string %d", term, i)
				}
				if v.(string) != string(z) {
					t.Errorf("term %s extended string cap %d (%s) should be %t", term, i, ic.extStringNames[i], v)
				}

				n, ok := ti.ExtStringNames[i]
				if !ok {
					t.Errorf("term %s missing extended string %d name", term, i)
				}
				if string(n) != ic.extStringNames[i] {
					t.Errorf("term %s extended string %d name should be '%s', got: '%s'", term, i, ic.extStringNames[i], string(n))
				}
			}
		})
	}
}

func loadInfocmpData(t *testing.T) error {
	// start loaders
	wg := new(sync.WaitGroup)
	termsCh, errs := make(chan string, 64), make(chan error, 64)
	for i := 0; i < runtime.NumCPU()*2; i++ {
		go infocmpLoader(wg, t, i, termsCh, errs)
	}

	for term := range terms(t) {
		wg.Add(1)
		termsCh <- term
	}
	defer close(termsCh)

	wg.Wait()

	var err error
	select {
	case err = <-errs:
	default:
	}

	return err
}

func infocmpLoader(wg *sync.WaitGroup, t *testing.T, id int, terms <-chan string, res chan<- error) {
	for term := range terms {
		ic, err := getInfocmpData(t, term)
		if err != nil {
			res <- fmt.Errorf("loader %d: %v", id, err)
			return
		}

		infocmpMap.Lock()
		infocmpMap.ic[term] = ic
		infocmpMap.Unlock()

		wg.Done()
	}
}

var (
	shortCapNameMap map[string]int
)

type infocmp struct {
	names          []string
	boolCaps       map[int]interface{}
	numCaps        map[int]interface{}
	stringCaps     map[int]interface{}
	extBoolCaps    map[int]interface{}
	extNumCaps     map[int]interface{}
	extStringCaps  map[int]interface{}
	extBoolNames   map[int]string
	extNumNames    map[int]string
	extStringNames map[int]string
}

func init() {
	shortCapNameMap = make(map[string]int)
	for _, z := range [][]string{boolCapNames[:], numCapNames[:], stringCapNames[:]} {
		for i := 0; i < len(z); i += 2 {
			shortCapNameMap[z[i+1]] = i / 2
		}
	}
}

var (
	staticCharRE = regexp.MustCompile(`(?m)^static\s+char\s+(.*)\s*\[\]\s*=\s*(".*");$`)
)

func getInfocmpData(t *testing.T, term string) (*infocmp, error) {
	c := exec.Command("/usr/bin/infocmp", "-E", "-x")
	c.Env = []string{"TERM=" + term}

	buf, err := c.CombinedOutput()
	if err != nil {
		t.Logf("shell error (TERM=%s):\n%s\n", term, string(buf))
		return nil, err
	}

	// read static strings
	m := staticCharRE.FindAllStringSubmatch(string(buf), -1)
	if !strings.HasSuffix(strings.TrimSpace(m[0][1]), "_alias_data") {
		return nil, errors.New("missing _alias_data")
	}

	// some names have " in them, and infocmp -E doesn't correctly escape them
	names, err := strconv.Unquote(`"` + strings.Replace(m[0][2][1:len(m[0][2])-1], `"`, `\"`, -1) + `"`)
	if err != nil {
		return nil, fmt.Errorf("could not unquote _alias_data: %v", err)
	}

	ic := &infocmp{
		names:          strings.Split(names, "|"),
		boolCaps:       make(map[int]interface{}),
		numCaps:        make(map[int]interface{}),
		stringCaps:     make(map[int]interface{}),
		extBoolCaps:    make(map[int]interface{}),
		extNumCaps:     make(map[int]interface{}),
		extStringCaps:  make(map[int]interface{}),
		extBoolNames:   make(map[int]string),
		extNumNames:    make(map[int]string),
		extStringNames: make(map[int]string),
	}

	// load string cap data
	caps := make(map[string]string, len(m))
	for i, s := range m[1:] {
		k := strings.TrimSpace(s[1])
		idx := strings.LastIndex(k, "_s_")
		if idx == -1 {
			return nil, fmt.Errorf("string cap %d (%s) does not contain _s_", i, k)
		}

		v, err := strconv.Unquote(s[2])
		if err != nil {
			return nil, fmt.Errorf("could not unquote %d: %v", i, err)
		}
		caps[k] = v
	}

	// extract the values
	for _, err := range []error{
		processSect(buf, caps, ic.boolCaps, ic.extBoolCaps, ic.extBoolNames, boolSectRE),
		processSect(buf, caps, ic.numCaps, ic.extNumCaps, ic.extNumNames, numSectRE),
		processSect(buf, caps, ic.stringCaps, ic.extStringCaps, ic.extStringNames, stringSectRE),
	} {
		if err != nil {
			return nil, err
		}
	}

	return ic, nil
}

// regexp's used by processSect.
var (
	boolSectRE   = regexp.MustCompile(`_bool_data\[\]\s*=\s*{`)
	numSectRE    = regexp.MustCompile(`_number_data\[\]\s*=\s*{`)
	stringSectRE = regexp.MustCompile(`_string_data\[\]\s*=\s*{`)
	endSectRE    = regexp.MustCompile(`(?m)^};$`)

	capValuesRE = regexp.MustCompile(`(?m)^\s+/\*\s+[0-9]+:\s+([^\s]+)\s+\*/\s+(.*),$`)
	numRE       = regexp.MustCompile(`^[0-9]+$`)
	absCanRE    = regexp.MustCompile(`^(ABSENT|CANCELLED)_(BOOLEAN|NUMERIC|STRING)$`)
)

// processSect processes a text section in the infocmp C export.
func processSect(buf []byte, caps map[string]string, xx, yy map[int]interface{}, extn map[int]string, sectRE *regexp.Regexp) error {
	var err error

	// extract section
	start := sectRE.FindIndex(buf)
	if start == nil || len(start) != 2 {
		return fmt.Errorf("could not find sect (%s)", sectRE)
	}
	end := endSectRE.FindIndex(buf[start[1]:])
	if end == nil || len(end) != 2 {
		return fmt.Errorf("could not find end of section (%s)", sectRE)
	}
	buf = buf[start[1] : start[1]+end[0]]

	// load caps
	m := capValuesRE.FindAllStringSubmatch(string(buf), -1)
	var extc int
	for i, s := range m {
		var skip bool

		// determine target
		target := xx
		k, ok := shortCapNameMap[s[1]]
		if !ok {
			target, k, extn[extc] = yy, extc, s[1]
			extc++
		}

		// get cap value
		var v interface{}
		switch {
		case s[2] == "TRUE" || s[2] == "FALSE":
			v = s[2] == "TRUE"

		case numRE.MatchString(s[2]):
			var j int64
			j, err = strconv.ParseInt(s[2], 10, 16)
			if err != nil {
				return fmt.Errorf("line %d could not parse: %v", i, err)
			}
			v = int(j)

		case absCanRE.MatchString(s[2]):
			if !ok { // absent/canceled extended cap
				if strings.HasSuffix(s[2], "NUMERIC") {
					v = -1
				} else {
					skip = true
				}
			}

		default:
			v, ok = caps[s[2]]
			if !ok {
				return fmt.Errorf("cap '%s' not defined in cap table", s[2])
			}
		}

		if !skip {
			target[k] = v
		}
	}

	return nil
}

module naive.systems/analyzer

go 1.18

require (
	github.com/bmatcuk/doublestar/v4 v4.0.2
	github.com/golang/glog v1.0.0
	github.com/google/shlex v0.0.0-20191202100458-e7afc7fbc510
	github.com/google/uuid v1.3.0
	github.com/hhatto/gocloc v0.4.2
	github.com/lib/pq v1.10.6
	github.com/libgit2/git2go/v33 v33.0.7
	github.com/securego/gosec/v2 v2.11.0
	golang.org/x/exp v0.0.0-20220518171630-0b5c67f07fdf
	golang.org/x/text v0.13.0
	golang.org/x/tools v0.14.0
	google.golang.org/protobuf v1.27.1
	gopkg.in/yaml.v2 v2.4.0
	honnef.co/go/tools v0.3.2
)

require (
	github.com/BurntSushi/toml v0.4.1 // indirect
	github.com/go-enry/go-enry/v2 v2.7.2 // indirect
	github.com/go-enry/go-oniguruma v1.2.1 // indirect
	github.com/nbutton23/zxcvbn-go v0.0.0-20210217022336-fa2cb2858354 // indirect
	golang.org/x/crypto v0.14.0 // indirect
	golang.org/x/exp/typeparams v0.0.0-20220218215828-6cf2b201936e // indirect
	golang.org/x/mod v0.13.0 // indirect
	golang.org/x/sys v0.13.0 // indirect
)

replace (
	github.com/BurntSushi/toml => ./third_party/github.com/BurntSushi/toml
	github.com/bmatcuk/doublestar/v4 => ./third_party/github.com/bmatcuk/doublestar.v4
	github.com/davecgh/go-spew => ./third_party/github.com/davecgh/go-spew
	github.com/go-enry/go-enry/v2 => ./third_party/github.com/go-enry/go-enry.v2
	github.com/go-enry/go-oniguruma => ./third_party/github.com/go-enry/go-oniguruma
	github.com/golang/glog => ./third_party/github.com/golang/glog
	github.com/golang/protobuf => ./third_party/github.com/golang/protobuf
	github.com/google/go-cmp => ./third_party/github.com/google/go-cmp
	github.com/google/shlex => ./third_party/github.com/google/shlex
	github.com/google/uuid => ./third_party/github.com/google/uuid
	github.com/gookit/color => ./third_party/github.com/gookit/color
	github.com/hhatto/gocloc => ./third_party/github.com/hhatto/gocloc
	github.com/jessevdk/go-flags => ./third_party/github.com/jessevdk/go-flags
	github.com/kr/pretty => ./third_party/github.com/kr/pretty
	github.com/kr/pty => ./third_party/github.com/kr/pty
	github.com/kr/text => ./third_party/github.com/kr/text
	github.com/lib/pq => ./third_party/github.com/lib/pq
	github.com/libgit2/git2go/v33 => ./third_party/github.com/libgit2/git2go.v33
	github.com/mozilla/tls-observatory => ./third_party/github.com/mozilla/tls-observatory
	github.com/nbutton23/zxcvbn-go => ./third_party/github.com/nbutton23/zxcvbn-go
	github.com/onsi/ginkgo/v2 => ./third_party/github.com/onsi/ginkgo.v2
	github.com/onsi/gomega => ./third_party/github.com/onsi/gomega
	github.com/pmezard/go-difflib => ./third_party/github.com/pmezard/go-difflib
	github.com/securego/gosec/v2 => ./third_party/github.com/securego/gosec.v2
	github.com/spf13/afero => ./third_party/github.com/spf13/afero
	github.com/stretchr/objx => ./third_party/github.com/stretchr/objx
	github.com/stretchr/testify => ./third_party/github.com/stretchr/testify
	github.com/xo/terminfo => ./third_party/github.com/xo/terminfo
	github.com/yuin/goldmark => ./third_party/github.com/yuin/goldmark
	golang.org/x/crypto => ./third_party/golang.org/x/crypto
	golang.org/x/exp => ./third_party/golang.org/x/exp
	golang.org/x/exp/typeparams => ./third_party/golang.org/x/exp/typeparams
	golang.org/x/mod => ./third_party/golang.org/x/mod
	golang.org/x/net => ./third_party/golang.org/x/net
	golang.org/x/sync => ./third_party/golang.org/x/sync
	golang.org/x/sys => ./third_party/golang.org/x/sys
	golang.org/x/term => ./third_party/golang.org/x/term
	golang.org/x/text => ./third_party/golang.org/x/text
	golang.org/x/tools => ./third_party/golang.org/x/tools
	golang.org/x/xerrors => ./third_party/golang.org/x/xerrors
	google.golang.org/protobuf => ./third_party/google.golang.org/protobuf
	gopkg.in/check.v1 => ./third_party/gopkg.in/check.v1
	gopkg.in/yaml.v2 => ./third_party/gopkg.in/yaml.v2
	gopkg.in/yaml.v3 => ./third_party/gopkg.in/yaml.v3
	honnef.co/go/tools => ./third_party/honnef.co/go/tools
)

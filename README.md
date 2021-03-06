# Hash Chain

## Concepts

The idea of a hash chain is simple: you start with a base (could be a
password, or just a number, or some other data) and hash it. You then take the
result and hash that too. You continue hashing the results repeatedly, till
you have a series of hashes like this:

    Base -> Hash0 = H(Base) -> Hash1 = H(Hash0) -> ... -> HashN = H(HashN-1)

The exciting property of these hash chains is that given the last hash in the
chain, HashN, it is very difficult to determine any of the previous hashes, or
the base. However, given the last hash, it is trivial to verify whether another
hash is part of the chain.

This means that a hash chain has the potential to be a limited source of
authentication. You can deploy a resource in public along with the last hash
of the chain. Then you can give commands to this resource, passing along each
previous hash as authentication of your identity.

## Build instructions
```shell
$ make
```

### Usage

To create a hash chain, the arguments are:
```shell
$ ./hashchain create -a ALGORITHM -l LENGTH SEED
```
or 
```shell
$ ./hashchain create -a ALGORITHM -i INDEX -s SIZE SEED
```

Simple example:
```shell
$ ./hashchain create -a sha256 -l 10 "my secret password"
```
or equivalently,
```shell
$ ./hashchain create -a sha256 -i 1 -s 10 "my secret password"
```

Alternatively, use built-in configurations:
```shell
$ make gen
```

It randomly generates a chain of length 10 using sha256 and saves it to the
file `chains`.

Each line of the output is base64 encoded data which hashes to the next line.

### Verify

Say that you have the last two hashes from the previous example:
```shell
$ tail -n 2 chains
FBxCC4r4/u9oyBtuF3sets/MpX38yGPHkyL5rtaGB58=
fdW9x8zM1ztLel4upwt2qW8x4EFw/WEfBOiXBiyEcuk=
```

To verify if two hashes are in the same chain, the arguments are:
```shell
$ ./hashchain verify -a ALGORITHM -q QUERY -n ANCHOR [-r MAX_RANGE]
```

You can verify with the command:
```shell
$ ./hashchain verify -a sha256 \
              -q FBxCC4r4/u9oyBtuF3sets/MpX38yGPHkyL5rtaGB58= \
              -n fdW9x8zM1ztLel4upwt2qW8x4EFw/WEfBOiXBiyEcuk= \
              -r 2
success
```

The verify command writes "success" and returns 0 if the hashes verify, and
writes "failure" and returns non-0 if they don't.

### Use case: Document Attestation
You can use these scripts to manipulate the PDF files. [exiftool](https://www.sno.phy.queensu.ca/~phil/exiftool/) and [openssl](https://www.openssl.org/) should be installed first in order to use these scripts.

You can sign a pdf file with the command:
```shell
$ scripts/sign.sh PDF_FILE [SEED] [ALGO]
```
Note that `SEED` is necessary when initializing the first hash, and the default `ALGO`  is sha256 if not specified manually.

To verify if the given inputs are in the same chain, you can use the command:
```shell
$ scripts/verify.sh INPUT_PDF ANCHOR [ALGO] [RANGE]
```
Note that `ALGO` is sha256 and `RANGE` is 10 by default. 
`INPUT_PDF` and `ANCHOR` can be either base64 encoded hash or pdf file.

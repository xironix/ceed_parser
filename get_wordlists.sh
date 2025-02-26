#!/bin/zsh
curl -s "https://raw.githubusercontent.com/trezor/trezor-firmware/master/crypto/bip39/wordlist/french.txt" > data/french.txt
curl -s "https://raw.githubusercontent.com/trezor/trezor-firmware/master/crypto/bip39/wordlist/italian.txt" > data/italian.txt
wc -l data/french.txt data/italian.txt 
#!/bin/zsh
# Enhanced script to download both BIP-39 and Monero wordlists for all supported languages

# Create directory if it doesn't exist
mkdir -p data

# Base URLs
TREZOR_BASE="https://raw.githubusercontent.com/trezor/trezor-firmware/master/crypto/bip39/wordlist"
MONERO_BASE="https://raw.githubusercontent.com/monero-project/monero/master/src/mnemonics"

# BIP-39 Wordlists (Standard)
echo "⬇️ Downloading BIP-39 wordlists..."
languages=("english" "spanish" "french" "italian" "portuguese" "czech" "japanese" "korean" "chinese_simplified" "chinese_traditional")

for lang in "${languages[@]}"; do
  echo "  🔄 Downloading BIP-39 $lang wordlist..."
  curl -s "${TREZOR_BASE}/${lang}.txt" > "data/${lang}.txt"
  count=$(wc -l < "data/${lang}.txt")
  echo "  ✅ BIP-39 $lang: $count words"
done

# Monero Wordlists 
echo "⬇️ Downloading Monero wordlists..."
monero_languages=("english" "spanish" "portuguese" "french" "italian" "german" "russian" "japanese" "chinese_simplified" "dutch" "esperanto" "lojban")

for lang in "${monero_languages[@]}"; do
  echo "  🔄 Downloading Monero $lang wordlist..."
  
  # For Monero, we need to extract the wordlist from the header files
  curl -s "${MONERO_BASE}/${lang}.h" | grep -E '^\s+"[a-z]+",$' | sed 's/^\s\+"//;s/",$//g' > "data/monero_${lang}.txt"
  
  count=$(wc -l < "data/monero_${lang}.txt")
  echo "  ✅ Monero $lang: $count words"
done

# Verify all files were downloaded successfully
echo "🔍 Verifying all wordlists..."
total_files=$(ls data/*.txt | wc -l)
echo "📊 Total wordlist files: $total_files"

# Check specific important files
for lang in "english" "spanish" "french"; do
  if [ -s "data/${lang}.txt" ]; then
    echo "✅ BIP-39 ${lang} wordlist exists and is not empty"
  else
    echo "❌ ERROR: BIP-39 ${lang} wordlist is missing or empty!"
  fi
  
  if [ -s "data/monero_${lang}.txt" ]; then
    echo "✅ Monero ${lang} wordlist exists and is not empty"
  else
    echo "❌ ERROR: Monero ${lang} wordlist is missing or empty!"
  fi
done

echo "✨ All wordlists downloaded successfully! ✨"
echo "🧠 Ready for mnemonic processing and benchmarking." 
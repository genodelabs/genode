" Vim syntax file
" Language:	Genode log output
" Maintainer:	Norman Feske <norman.feske@genode-labs.com>
"
if exists('b:current_syntax')
  finish
endif
let b:current_syntax = 'genode_log'

syntax match  logEsc  /\[\d\+m/
syntax region logError   start=/\[31m/ end=/$/ contains=logEsc
syntax region logWarning start=/\[34m/ end=/$/ contains=logEsc
syntax region logLabel matchgroup=logBracket start=/\[/ end=/\]/ contained
syntax match  logTime /^\d\+[.]\d\+/ contained
syntax match  logTag /^[^\]]\+\]/ contains=logLabel,logTime

highlight link logError   Error
highlight link logWarning Comment
highlight link logLabel   Constant
highlight link logBracket Statement
highlight link logTime    Default
highlight link logEsc     Comment

" Vim syntax file
" Language:	Human-Inclined Data
" Maintainer:	Norman Feske <norman.feske@genode-labs.com>

if exists('b:current_syntax')
  finish
endif
let b:current_syntax = 'hid'

syntax match  hidPipe /^\([ |] \)*| /
syntax match  hidNode /^\([ |] \)*+ [a-z][a-z0-9_\-]*/
syntax match  hidPipe / [|] /
syntax match  hidNode /^[a-z][a-z0-9_\-]*/
syntax match  hidNode / [|] + [a-z][a-z0-9_\-]*/
syntax match  hidAttr /[a-z][a-z0-9_\-]*:/
syntax match  hidError /^ \([ |] \)*\([|].\)\+/
syntax region hidComment matchgroup=hidAnchor start=/^ *[.] / end=/$/
syntax region hidComment matchgroup=hidAnchor start=/ | [.] / end=/$/
syntax region hidQuoted  matchgroup=hidAnchor start=/^ *: /   end=/$/
syntax region hidQuoted  matchgroup=hidAnchor start=/ | : /   end=/$/

syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{0}x /  end=/^[ |]\{0,0}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{2}x /  end=/^[ |]\{0,2}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{4}x /  end=/^[ |]\{0,4}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{6}x /  end=/^[ |]\{0,6}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{8}x /  end=/^[ |]\{0,8}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{10}x / end=/^[ |]\{0,10}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{12}x / end=/^[ |]\{0,12}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{14}x / end=/^[ |]\{0,14}[^ |]/me=s-1
syntax region hidDisabled matchgroup=hidAnchor start=/^[ |]\{16}x / end=/^[ |]\{0,16}[^ |]/me=s-1

highlight link hidComment Comment
highlight link hidQuoted Identifier
highlight link hidNode Statement
highlight link hidPipe Statement
highlight link hidAttr Constant
highlight link hidDisabled Comment
highlight link hidAnchor Statement
highlight link hidError Error

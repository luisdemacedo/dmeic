;;; gdb-wolfram-activate.el --- activate gdb-wolfram  -*- lexical-binding: t; -*-

;; Copyright (C) 2022  João O'Neill Cortes

;; Author: João O'Neill Cortes <joaooneillcortes@outlook.pt>
;; Keywords: tools

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:

;; 

;;; Code:

(load-file "~/OneDrive/Documents/atelier/comint-gdb-filters/gdb-wolfram.el")
(load-file "~/.emacs.d/opus/packages/joc-gdb-mi/joc-gdb-mi.el")
(setq gdbmi-debug-mode "*gdbmi-probe*")
;; (setq gdbmi-debug-mode nil)


(provide 'gdb-wolfram-activate)
;;; gdb-wolfram-activate.el ends here

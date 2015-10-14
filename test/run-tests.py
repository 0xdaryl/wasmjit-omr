#!/usr/bin/env python
# Copyright 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import fnmatch
import multiprocessing
import os
import Queue
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT_DIR = os.path.dirname(SCRIPT_DIR)
DEFAULT_EXE = os.path.join(REPO_ROOT_DIR, 'out', 'sexpr-wasm')
DEFAULT_TIMEOUT = 2 # seconds
SLOW_TIMEOUT_MULTIPLIER = 2


class Error(Exception):
  pass


def AsList(value):
  if value is None:
    return []
  elif type(value) is list:
    return value
  else:
    return [value]


def Indent(s, spaces):
  return ''.join(' '*spaces + l for l in s.splitlines(1))


def DiffLines(expected, actual):
  expected_lines = expected.splitlines(1)
  actual_lines = actual.splitlines(1)
  return list(difflib.unified_diff(expected_lines, actual_lines,
                                   fromfile='expected', tofile='actual'))


def FindTestFiles(directory, ext, filter_pattern_re):
  tests = []
  for root, dirs, files in os.walk(directory):
    for f in files:
      path = os.path.join(root, f)
      if os.path.splitext(f)[1] == ext:
        tests.append(os.path.relpath(path, SCRIPT_DIR))
  tests.sort()
  return [test for test in tests if re.match(filter_pattern_re, test)]


class TestInfo(object):
  def __init__(self):
    self.name = ''
    self.filename = ''
    self.header = []
    self.input_file = ''
    self.input_ = []
    self.expected_stdout = ''
    self.expected_stderr = ''
    self.exe = None
    self.flags = []
    self.expected_error = 0
    self.slow = False
    self.skip = False

  def Parse(self, filename):
    self.name = filename
    self.filename = os.path.join(SCRIPT_DIR, filename)

    with open(self.filename) as f:
      seen_keys = set()
      state = 'header'
      empty = True
      header_lines = []
      input_lines = []
      stdout_lines = []
      stderr_lines = []
      for line in f.readlines():
        empty = False
        m = re.match(r'\s*#(.*)$', line)
        if m:
          directive = m.group(1).strip()
          if directive.lower() == 'stderr:':
            state = 'stderr'
            continue
          elif directive.lower() == 'stdout:':
            state = 'stdout'
            continue

          if state == 'header':
            key, value = directive.split(':', 1)
            key = key.strip().lower()
            value = value.strip()
            if key in seen_keys:
              raise Error('%s already set' % key)
            seen_keys.add(key)
            if key == 'exe':
              self.exe = value
            elif key == 'stdin_file':
              self.input_file = value
            elif key == 'flags':
              self.flags = shlex.split(value)
            elif key == 'error':
              self.expected_error = int(value)
            elif key == 'slow':
              self.slow = True
            elif key == 'skip':
              self.skip = True
            else:
              raise Error('Unknown directive: %s' % key)
        elif state == 'header':
          state = 'input'

        if state == 'header':
          header_lines.append(line)
        if state == 'input':
          if self.input_file:
            raise Error('Can\'t have input_file and input')
          input_lines.append(line)
        elif state == 'stderr':
          stderr_lines.append(line)
        elif state == 'stdout':
          stdout_lines.append(line)
    if empty:
      raise Error('empty test file')

    self.header = ''.join(header_lines)
    self.input_ = ''.join(input_lines)
    self.expected_stdout = ''.join(stdout_lines)
    self.expected_stderr = ''.join(stderr_lines)

  def GetExecutable(self, override_exe):
    # If the test overrides the executable, we assume that it knows best, so we
    # don't allow the commandline to override it. Instead, we assume that
    # executable will take the "-e" flag to specify the sexpr-wasm executable
    # to run.
    if self.exe:
      exe = os.path.join(REPO_ROOT_DIR, self.exe)
    elif override_exe:
      exe = override_exe
    else:
      exe = DEFAULT_EXE
    return os.path.abspath(os.path.join(SCRIPT_DIR, exe))

  def GetCommand(self, filename, override_exe=None):
    cmd = [self.GetExecutable(override_exe)]
    cmd += self.flags
    # See comment in GetExecutable above
    if self.exe and override_exe:
      cmd += ['-e', override_exe]
    cmd += [filename]
    return cmd

  def Run(self, timeout, temp_dir, override_exe=None):
    if self.input_file:
      file_path = os.path.join(temp_dir, self.input_file)
    else:
      file_path = os.path.join(temp_dir, self.name)
    file_dir = os.path.dirname(file_path)
    try:
      os.makedirs(file_dir)
    except OSError as e:
      if not os.path.isdir(file_dir):
        raise

    if self.slow:
      timeout *= SLOW_TIMEOUT_MULTIPLIER

    process = None
    # Cheesy way to be able to set is_timeout from inside KillProcess
    is_timeout = [False]
    def KillProcess(timeout=True):
      if process:
        try:
          os.killpg(os.getpgid(process.pid), 15)
        except OSError:
          pass
        is_timeout[0] = timeout

    file_ = open(file_path, 'w')
    try:
      # add an empty line for each header line so the line numbers match
      if self.input_file:
        with open(self.input_file) as input_file:
          file_.write(input_file.read())
      else:
        file_.write('\n' * self.header.count('\n'))
        file_.write(self.input_)
      file_.flush()
      # make filenames relative to temp_dir so the autogenerated name is not
      # included in the error output.
      rel_file_path = os.path.relpath(file_.name, temp_dir)
      cmd = self.GetCommand(rel_file_path, override_exe)
      try:
        start_time = time.time()
        # http://stackoverflow.com/a/10012262: subprocess with a timeout
        # http://stackoverflow.com/a/22582602: kill subprocess and children
        process = subprocess.Popen(cmd, cwd=temp_dir, stdout=subprocess.PIPE,
                                                      stderr=subprocess.PIPE,
                                   preexec_fn=os.setsid)
        timer = threading.Timer(timeout, KillProcess)
        try:
          timer.start()
          stdout, stderr = process.communicate()
        finally:
          timer.cancel()
        if is_timeout[0]:
          raise Error('TIMEOUT\nSTDOUT:\n%s\nSTDERR:\n%s\n' % (stdout, stderr))
        duration = time.time() - start_time
      except OSError as e:
        raise Error(str(e))
      finally:
        KillProcess(False)
    finally:
      file_.close()

    return stdout, stderr, process.returncode, duration

  def Rebase(self, stdout, stderr):
    with open(self.filename, 'w') as f:
      f.write(self.header)
      f.write(self.input_)
      if stderr:
        f.write('# STDERR:\n')
        f.write(stderr)
      if stdout:
        f.write('# STDOUT:\n')
        f.write(stdout)

  def Diff(self, stdout, stderr):
    msg = ''
    if self.expected_stderr != stderr:
      diff_lines = DiffLines(self.expected_stderr, stderr)
      msg += 'STDERR MISMATCH:\n' + ''.join(diff_lines)

    if self.expected_stdout != stdout:
      diff_lines = DiffLines(self.expected_stdout, stdout)
      msg += 'STDOUT MISMATCH:\n' + ''.join(diff_lines)

    if msg:
      raise Error(msg)


class Status(object):
  def __init__(self, verbose):
    self.verbose = verbose
    self.start_time = None
    self.last_length = 0
    self.last_finished = None
    self.passed = 0
    self.failed = 0
    self.total = 0
    self.failed_tests = []

  def Start(self, total):
    self.total = total
    self.start_time = time.time()

  def Passed(self, info, duration):
    self.passed += 1
    if self.verbose:
      sys.stderr.write('+ %s (%.3fs)\n' % (info.name, duration))
    else:
      self.Clear()
      self._PrintShortStatus(info)
      sys.stderr.flush()

  def Failed(self, info, error_msg):
    self.failed += 1
    self.failed_tests.append(info)
    self.Clear()
    sys.stderr.write('- %s\n%s\n' % (info.name, Indent(error_msg, 2)))

  def Skipped(self, info):
    if self.verbose:
      sys.stderr.write('. %s (skipped)\n' % info.name)

  def Timeout(self):
    self._PrintShortStatus(self.last_finished)

  def Print(self):
    self._PrintShortStatus(None)
    sys.stderr.write('\n')

  def _PrintShortStatus(self, info):
    total_duration = time.time() - self.start_time
    name = info.name if info else ''
    if self.total:
      percent = 100 * (self.passed + self.failed) / self.total
    else:
      percent = 100
    status = '[+%d|-%d|%%%d] (%.2fs) %s\r' % (self.passed, self.failed,
                                              percent, total_duration, name)
    self.last_length = len(status)
    self.last_finished = info
    sys.stderr.write(status)

  def Clear(self):
    if not self.verbose:
      sys.stderr.write('%s\r' % (' ' * self.last_length))


def GetAllTestInfo(test_names, status):
  infos = []
  for test_name in test_names:
    info = TestInfo()
    try:
      info.Parse(test_name)
      infos.append(info)
    except Error as e:
      status.Failed(info, str(e))

  return infos


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-e', '--executable', help='override executable.')
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages.',
                      action='store_true')
  parser.add_argument('-l', '--list', help='list all tests.',
                      action='store_true')
  parser.add_argument('--list-exes',
                      help='list all executables needed for the tests.',
                      action='store_true')
  parser.add_argument('-r', '--rebase',
                      help='rebase a test to its current output.',
                      action='store_true')
  parser.add_argument('-j', '--jobs', help='number of jobs to use to run tests',
                      type=int, default=multiprocessing.cpu_count())
  parser.add_argument('-t', '--timeout', type=float, default=DEFAULT_TIMEOUT,
                      help='per test timeout in seconds')
  parser.add_argument('patterns', metavar='pattern', nargs='*',
                      help='test patterns.')
  options = parser.parse_args(args)

  if options.patterns:
    pattern_re = '|'.join(fnmatch.translate('*%s*' % p)
                          for p in options.patterns)
  else:
    pattern_re = '.*'

  test_names = FindTestFiles(SCRIPT_DIR, '.txt', pattern_re)
  if options.list:
    for test_name in test_names:
      print test_name
    return 0
  if not test_names:
    print 'no tests match that filter'
    return 1

  if options.executable:
    if not os.path.exists(options.executable):
      parser.error('executable %s does not exist' % options.executable)
    options.executable = os.path.abspath(options.executable)

  isatty = os.isatty(1)

  status = Status(options.verbose)
  infos = GetAllTestInfo(test_names, status)

  if options.list_exes:
    exes = set([info.exe for info in infos])
    if None in exes:
      exes.remove(None)
      exes.add(os.path.relpath(DEFAULT_EXE, os.getcwd()))
    print '\n'.join(exes)
    return 0

  inq = multiprocessing.Queue()
  test_count = 0
  for info in infos:
    if info.skip:
      status.Skipped(info)
      continue
    inq.put(info)
    test_count += 1

  outq = multiprocessing.Queue()
  num_proc = options.jobs
  processes = []
  status.Start(test_count)

  def Worker(i, options, inq, outq):
    try:
      while True:
        try:
          info = inq.get(False)
          try:
            out = info.Run(options.timeout, temp_dir, options.executable)
          except Exception as e:
            outq.put((info, e))
            continue
          outq.put((info, out))
        except Queue.Empty:
          # Seems this can be fired even when the queue isn't actually empty.
          # Double-check, via inq.empty()
          if inq.empty():
            break
    except KeyboardInterrupt:
      pass

  temp_dir = tempfile.mkdtemp(prefix='sexpr-wasm-')
  try:
    for i, p in enumerate(range(num_proc)):
      proc = multiprocessing.Process(target=Worker,
                                     args=(i, options, inq, outq))
      processes.append(proc)
      proc.start()

    finished_tests = 0
    while finished_tests < test_count:
      try:
        info, result = outq.get(True, 0.01)
      except Queue.Empty:
        status.Timeout()
        continue

      finished_tests += 1
      try:
        if isinstance(result, Exception):
          raise result

        stdout, stderr, returncode, duration = result
        if returncode != info.expected_error:
          # This test has already failed, but diff it anyway.
          msg = 'expected error code %d, got %d.' % (info.expected_error,
                                                     returncode)
          try:
            info.Diff(stdout, stderr)
          except Error as e:
            msg += '\n' + str(e)
          raise Error(msg)
        else:
          if options.rebase:
            info.Rebase(stdout, stderr)
          else:
            info.Diff(stdout, stderr)
          status.Passed(info, duration)
      except Exception as e:
        status.Failed(info, str(e))
  except KeyboardInterrupt:
    for proc in processes:
      proc.join()
  finally:
    for proc in processes:
      proc.terminate()
      proc.join()
    shutil.rmtree(temp_dir)

  status.Clear()

  ret = 0

  if status.failed:
    sys.stderr.write('**** FAILED %s\n' % ('*' * (80 - 14)))
    for info in status.failed_tests:
      sys.stderr.write('- %s\n' % info.name)
    ret = 1

  status.Print()
  return ret


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)

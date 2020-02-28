# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess


def main(output, header, moc, *flags):
    result = subprocess.run(
        [moc, "--ignore-option-clashes", *flags, header],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    output.write(result.stdout)

#!/bin/bash
#
#      -*- OpenSAF  -*-
#
# (C) Copyright 2011 The OpenSAF Foundation
# Copyright Ericsson AB 2017 - All Rights Reserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
# under the GNU Lesser General Public License Version 2.1, February 1999.
# The complete license can be accessed from the following location:
# http://opensource.org/licenses/lgpl-license.php
# See the Copying file included with the OpenSAF distribution for full
# licensing terms.
#
# Author(s): Wind River Systems
#            Ericsson AB
#

if [[ -z "$EDITOR" ]]; then
	EDITOR="vi"
fi

dryrun=0; force=0

usage="Usage: submit-review.sh [-t] [-f] [-d dest]"

while getopts ":tfd:" opt; do
	case $opt in
		t ) dryrun=1 ;;
		f ) force=1 ;;
		d ) dest=$OPTARG ;;
		\?) echo "$usage"
		exit 1 ;;
	esac
done

shift $((OPTIND -1))

GIT=$(which git)
if [[ -z "$GIT" ]]; then
    echo "*** ERROR: The git command not found"
    echo "           Please run: sudo apt-get install git"
    exit 1
fi

if ! "$GIT" send-email --help > /dev/null 2>&1; then
        echo "*** ERROR: The git send-email command isn't installed on your system"
        echo "           Please run: sudo apt-get install git-email"
        exit 1
fi

if ! user_email=$("$GIT" config user.email); then
        echo "*** ERROR: No E-mail address has been configured for GIT"
        echo '           Please run: git config --global user.email "your.email@company.com"'
        exit 1
fi

if ! user_name=$("$GIT" config user.name); then
        echo "*** ERROR: No name has been configured for GIT"
        echo '           Please run: git config --global user.name "Your Name"'
        exit 1
fi

if [[ -z "$OSAF_SOURCEFORGE_USER_ID" ]]; then
    echo "OSAF_SOURCEFORGE_USER_ID is not set. Please set it to your SourceForge user-id."
    exit 1
fi

if [[ -z "$OSAF_SOURCEFORGE_GIT_REPO_ID" ]]; then
    OSAF_SOURCEFORGE_GIT_REPO_ID=$OSAF_SOURCEFORGE_USER_ID
fi

if [[ -z "$OSAF_TEST_WORKDIR" ]]; then
    export OSAF_TEST_WORKDIR=$HOME/osaf_test_workdir
fi

common_repo="ssh://${OSAF_SOURCEFORGE_USER_ID}@git.code.sf.net/p/opensaf/code"
personal_repo="ssh://${OSAF_SOURCEFORGE_USER_ID}@git.code.sf.net/u/${OSAF_SOURCEFORGE_GIT_REPO_ID}/review"

branch=$("$GIT" symbolic-ref --quiet --short HEAD)

if [[ $(echo "$branch" | cut -c1-7) != "ticket-" ]]; then
    echo "*** ERROR: The name of the current GIT branch ($branch) does not have the format"
    echo "           ticket-XXXX, where XXXX is a number: it does not start with ticket-"
    exit 1
fi

ticket=$(echo "$branch" | cut -c8-)

if [[ -z "$ticket" ]]; then
    echo "*** ERROR: The name of the current GIT branch ($branch) does not have the format"
    echo "           ticket-XXXX, where XXXX is a number: XXXX is empty"
    exit 1
fi

if [[ -n $(echo "$ticket" | tr -d "0-9") ]]; then
    echo "*** ERROR: The name of the current GIT branch ($branch) does not have the format"
    echo "           ticket-XXXX, where XXXX is a number: XXXX is not a number"
    exit 1
fi

if [[ $("$GIT" status -s | wc -l) -ne 0 ]]; then
    "$GIT" status -s
    echo "*** ERROR: Your GIT repository is not clean; please commit or stash any local changes"
    exit 1
fi

echo "Fetching the latest from the develop branch at Source Forge"
"$GIT" fetch "$common_repo" develop || exit 1
fetch_head=$("$GIT" rev-parse FETCH_HEAD)
if ! develop_head=$("$GIT" rev-parse develop); then
        echo "No local branch with the name 'develop' exists."
        echo "Please run: git checkout develop"
        exit 1
fi
if [[ "$fetch_head" != "$develop_head" ]]; then
    echo "The develop branch is not up to date"
    echo "           Please run: git checkout develop && git pull"
    exit 1
fi

ticket_head=$("$GIT" rev-parse "$branch")
fork_point=$("$GIT" merge-base --fork-point develop "$branch")

if [[ "$develop_head" != "$fork_point" ]]; then
    echo "*** ERROR: The branch $branch must be rebased."
    echo "           Please run: git rebase develop"
    exit 1
fi

if [ -z "$dest" ]; then
	if ! rr=$(mktemp -d); then
		echo "$0: mktemp failed"
		exit 1
	fi
else
	rr=$dest
	if [ ! -d "$rr" ]; then
		if ! mkdir -p "$rr"; then
			echo "$0: mkdir $rr failed"
			exit 1
		fi
	fi
fi

rev=${fork_point}..${ticket_head}
"$GIT" log "$rev" --pretty='format:%s' > "$rr/shortlog.txt"
if [[ $(wc -c < "$rr/shortlog.txt") -eq 0 ]]; then
    echo "No revisions on branch $branch to submit for review..exiting."
    exit 0
fi
echo >> "$rr/shortlog.txt"
shortlog_entries=$(wc -l < "$rr/shortlog.txt")
if [[ "$shortlog_entries" -gt 25 ]]; then
    echo "------ short commit log ------"
    tail -30 "$rr/shortlog.txt"
    echo "------------------------------"
    echo "*** ERROR: More than 25 revisions on branch $branch to submit for review."
    exit 1
fi

grep -v -E "^[a-z]+: " "$rr/shortlog.txt" > "$rr/missing_component.txt"
if [[ $(wc -l < "$rr/missing_component.txt") -gt 0 ]]; then
    echo "------ short commit messages with missing component name ------"
    cat "$rr/missing_component.txt"
    echo "---------------------------------------------------------------"
    echo "*** ERROR: Component name missing in commit messages"
    exit 1
fi

grep -v -E " \[#$ticket\]\$" "$rr/shortlog.txt" > "$rr/missing_ticket.txt"
if [[ $(wc -l < "$rr/missing_ticket.txt") -gt 0 ]]; then
    echo "------ short commit messages with missing ticket number ------"
    cat "$rr/missing_ticket.txt"
    echo "--------------------------------------------------------------"
    echo "*** ERROR: Ticket number [#$ticket] missing in commit messages"
    exit 1
fi

summary=$(tail -1 "$rr/shortlog.txt")
if [ -z "$summary" ]; then
    summary="*** FILL ME ***"
fi

"$GIT" log "$rev" --pretty='format:%ae' > "$rr/shortlog.txt"
if [[ $(wc -c < "$rr/shortlog.txt") -eq 0 ]]; then
    echo "*** ERROR: Missing E-mail address in some commit messages"
    exit 0
fi
echo >> "$rr/shortlog.txt"
email_entries=$(wc -l < "$rr/shortlog.txt")
if [[ "$email_entries" -ne "$shortlog_entries" ]]; then
    echo "*** ERROR: Missing E-mail address in some commit messages"
    exit 1
fi
if grep -v -E "^$user_email\$" < "$rr/shortlog.txt"; then
        echo "*** ERROR: E-mail address $user_email missing in some commit messages"
        exit 1
fi

"$GIT" log "$rev" --pretty='format:%an' > "$rr/shortlog.txt"
if [[ $(wc -c < "$rr/shortlog.txt") -eq 0 ]]; then
    echo "*** ERROR: Missing user name in some commit messages"
    exit 0
fi
echo >> "$rr/shortlog.txt"
name_entries=$(wc -l < "$rr/shortlog.txt")
if [[ "$name_entries" -ne "$shortlog_entries" ]]; then
    echo "*** ERROR: Missing user name address in some commit messages"
    exit 1
fi
if grep -v -E "^$user_name\$" < "$rr/shortlog.txt"; then
        echo "*** ERROR: User name $user_name missing in some commit messages"
        exit 1
fi

git diff --find-renames "$rev" > "$rr/single_patch.txt"
if grep -E '^\+.*[[:space:]]$' "$rr/single_patch.txt"; then
    echo "*** ERROR: Patch adds trailing whitespace"
    exit 1
fi

patches_contain_long_lines=0
sed -i -e 's/\t/        /g' "$rr/single_patch.txt"
if grep --color=always -E '^\+[^#].{80}' "$rr/single_patch.txt"; then
    echo "*** ERROR: Patch adds lines exceeding the 80 character width limit"
    read -r -i no -p "Do you wish to send the patch for review anyway? " send_anyway
    if [[ "$send_anyway" != "yes" ]]; then
	exit 1
    fi
    patches_contain_long_lines=1
fi
rm "$rr/single_patch.txt" "$rr/shortlog.txt" "$rr/missing_ticket.txt" "$rr/missing_component.txt"

patches_are_untested=0
if [[ -f "$OSAF_TEST_WORKDIR/good_revisions/$ticket_head" ]]; then
    echo "Revision $ticket_head has passed the OpenSAF tests"
else
    if [[ "$force" -eq 1 ]]; then
	echo "Revision $ticket_head has NOT passed the OpenSAF tests, but -f flag was used"
	patches_are_untested=1
	summary="$summary (untested)"
    else
	echo "*** ERROR: Revision $ticket_head has NOT passed the OpenSAF tests"
        echo "           Please run: ./test.sh at the top of the source code repository"
	exit 1
    fi
fi

echo "Exporting revision(s) '$rev' for review:"
echo
git log "$rev"
echo

if [ $dryrun -eq 1 ]; then
	echo "***** Running in dry-run mode, nothing will be emailed *****"
fi

echo "Pushing branches $branch and develop to your personal SourceForge repository:"
if [ $dryrun -eq 1 ]; then
    echo "$GIT" push "$personal_repo" "$branch" develop
else
    if ! "$GIT" push "$personal_repo" "$branch" develop; then
        echo "*** ERROR: Could not push to $personal_repo"
        echo "           Please check that you have a GIT repository with the name 'review' at SourceForge"
	echo "           If $branch already exists because of an earlier review of the same ticket,"
	echo "           you must delete the branch using: git push --delete $personal_repo $branch"
        exit 1
    fi
fi

echo "The review package is exported into $rr"

"$GIT" format-patch --numbered --cover-letter -o "$rr" "$rev"

cat <<ETX >> "$rr/rr.patch"
Summary: $summary
Review request for Ticket(s): $ticket
Peer Reviewer(s): *** LIST THE TECH REVIEWER(S) / MAINTAINER(S) HERE ***
Pull request to: *** LIST THE PERSON WITH PUSH ACCESS HERE ***
Affected branch(es): develop
Development branch: $branch
Base revision: $develop_head
Personal repository: git://git.code.sf.net/u/${OSAF_SOURCEFORGE_GIT_REPO_ID}/review

--------------------------------
Impacted area       Impact y/n
--------------------------------
 Docs                    n
 Build system            n
 RPM/packaging           n
 Configuration files     n
 Startup scripts         n
 SAF services            n
 OpenSAF services        n
 Core libraries          n
 Samples                 n
 Tests                   n
 Other                   n

ETX

if [[ "$patches_contain_long_lines" -ne 0 ]]; then
	echo "NOTE: Patch(es) contain lines longer than 80 characers" >> "$rr/rr.patch"
fi
if [[ "$patches_are_untested" -ne 0 ]]; then
	echo "NOTE: Patch(es) are UNTESTED" >> "$rr/rr.patch"
fi

cat <<ETX >> "$rr/rr.patch"

Comments (indicate scope for each "y" above):
---------------------------------------------
*** EXPLAIN/COMMENT THE PATCH SERIES HERE ***

ETX
"$GIT" log --pretty=format:'revision %H%nAuthor:%x09%cn <%ce>%nDate:%x09%cD%n%n%B%n%n' "$rev" >> "$rr/rr.patch"

new=$(grep -E -A 3 -s '^new file mode ' "$rr"/*.patch | grep -s '\+++ b/' | awk -F "b/" '{ print $2 }' | sort -u)
if [ -n "$new" ]; then
	{
		echo ""
		echo "Added Files:"
		echo "------------"
		for l in $new; do
			echo " $l"
		done
		echo ""
	} >> "$rr/rr.patch"
fi

del=$(grep -E -A 2 -s '^deleted file mode ' "$rr"/*.patch | grep -s '\--- a/' | awk -F "a/" '{ print $2 }' | sort -u)
if [ -n "$del" ]; then
	{
		echo ""
		echo "Removed Files:"
		echo "--------------"
		for l in $del; do
			echo " $l"
		done
		echo ""
	} >> "$rr/rr.patch"
fi
cat <<ETX >> "$rr/rr.patch"

Complete diffstat:
------------------
ETX
"$GIT" diff --stat "$rev" >> "$rr/rr.patch"
cat <<ETX >> "$rr/rr.patch"


Testing Commands:
-----------------
*** LIST THE COMMAND LINE TOOLS/STEPS TO TEST YOUR CHANGES ***


Testing, Expected Results:
--------------------------
*** PASTE COMMAND OUTPUTS / TEST RESULTS ***


Conditions of Submission:
-------------------------
*** HOW MANY DAYS BEFORE PUSHING, CONSENSUS ETC ***


Arch      Built     Started    Linux distro
-------------------------------------------
mips        n          n
mips64      n          n
x86         n          n
x86_64      n          n
powerpc     n          n
powerpc64   n          n


Reviewer Checklist:
-------------------
[Submitters: make sure that your review doesn't trigger any checkmarks!]


Your checkin has not passed review because (see checked entries):

___ Your RR template is generally incomplete; it has too many blank entries
    that need proper data filled in.

___ You have failed to nominate the proper persons for review and push.

___ Your patches do not have proper short+long header

___ You have grammar/spelling in your header that is unacceptable.

___ You have exceeded a sensible line length in your headers/comments/text.

___ You have failed to put in a proper Trac Ticket # into your commits.

___ You have incorrectly put/left internal data in your comments/files
    (i.e. internal bug tracking tool IDs, product names etc)

___ You have not given any evidence of testing beyond basic build tests.
    Demonstrate some level of runtime or other sanity testing.

___ You have ^M present in some of your files. These have to be removed.

___ You have needlessly changed whitespace or added whitespace crimes
    like trailing spaces, or spaces before tabs.

___ You have mixed real technical changes with whitespace and other
    cosmetic code cleanup changes. These have to be separate commits.

___ You need to refactor your submission into logical chunks; there is
    too much content into a single commit.

___ You have extraneous garbage in your review (merge commits etc)

___ You have giant attachments which should never have been sent;
    Instead you should place your content in a public tree to be pulled.

___ You have too many commits attached to an e-mail; resend as threaded
    commits, or place in a public tree for a pull.

___ You have resent this content multiple times without a clear indication
    of what has changed between each re-send.

___ You have failed to adequately and individually address all of the
    comments and change requests that were proposed in the initial review.

___ You have a misconfigured ~/.gitconfig file (i.e. user.name, user.email etc)

___ Your computer have a badly configured date and time; confusing the
    the threaded patch review.

___ Your changes affect IPC mechanism, and you don't present any results
    for in-service upgradability test.

___ Your changes affect user manual and documentation, your patch series
    do not contain the patch that updates the Doxygen manual.

ETX

"$EDITOR" "$rr/rr.patch"

while [ -z "$subject" ]; do
        read -r -p "Subject: Review Request for " -e subject
done

while [ -z "$toline" ]; do
        read -r -p "To: " -e toline
done

"$GIT" format-patch --numbered --cover-letter --subject="PATCH" --to "$toline" --cc "opensaf-devel@lists.sourceforge.net" -o "$rr" "$rev"

sed -i -e "s/\*\*\* SUBJECT HERE \*\*\*/Review Request for ${subject//\//\\\/}/" "$rr/0000-cover-letter.patch"
sed -i -e '/^\*\*\* BLURB HERE \*\*\*$/,$d' "$rr/0000-cover-letter.patch"
cat "$rr/rr.patch" >> "$rr/0000-cover-letter.patch"
rm -f "$rr"/rr.patch*

if [ $dryrun -eq 1 ]; then
	echo "Email thread dumped into $rr/"
	$GIT send-email --dry-run --no-format-patch --to "$toline" --cc "opensaf-devel@lists.sourceforge.net" "$rr"
else
	$GIT send-email --no-format-patch --confirm=never --to "$toline" --cc "opensaf-devel@lists.sourceforge.net" "$rr"
	rm -f "$rr"/*.patch
	rmdir "$rr"
fi

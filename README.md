
                      =================================
                      Genode Operating System Framework
                      =================================


This is the source code of Genode, which is a framework for creating
component-based operating systems. It combines capability-based security,
microkernel technology, sandboxed device drivers, and virtualization with
a novel operating system architecture. For a general overview about the
architecture, please refer to the project's official website:

:Website for the Genode OS Framework:

  [https://genode.org/documentation/general-overview]

Genode-based operating systems can be compiled for a variety of kernels: Linux,
L4ka::Pistachio, L4/Fiasco, OKL4, NOVA, Fiasco.OC, seL4, and a custom "hw"
microkernel for running Genode without a 3rd-party kernel. Whereas the Linux
version serves us as development vehicle and enables us to rapidly develop the
generic parts of the system, the actual target platforms of the framework are
microkernels. There is no "perfect" microkernel - and neither should there be
one. If a microkernel pretended to be fit for all use cases, it wouldn't be
"micro". Hence, all microkernels differ in terms of their respective features,
complexity, and supported hardware architectures.

Genode allows for the use of each of the supported kernels with a rich set of
device drivers, protocol stacks, libraries, and applications in a uniform way.
For developers, the framework provides an easy way to target multiple different
kernels instead of tying the development to a particular kernel technology. For
kernel developers, Genode contributes advanced workloads, stress-testing their
kernel, and enabling a variety of application use cases that would not be
possible otherwise. For users and system integrators, it enables the choice of
the kernel that fits best with the requirements at hand for the particular
usage scenario.


Documentation
#############

The primary documentation is the book "Genode Foundations", which is available
on the front page of the Genode website:

:Download the book "Genode Foundations":

  [https://genode.org]

The book describes Genode in a holistic and comprehensive way. It equips you
with a thorough understanding of the architecture, assists developers with the
explanation of the development environment and system configuration, and
provides a look under the hood of the framework. Furthermore, it contains the
specification of the framework's programming interface.

The project has a quarterly release cycle. Each version is accompanied with
detailed release documentation, which is available at the documentation
section of the project website:

:Release documentation:

  [https://genode.org/documentation/release-notes/]


Directory overview
##################

The source tree is composed of the following subdirectories:

:'doc':

  This directory contains general documentation along with a comprehensive
  collection of release notes.

:'repos':

  This directory contains the source code, organized in so-called source-code
  repositories. Please refer to the README file in the 'repos' directory to
  learn more about the roles of the individual repositories.

:'tool':

  Source-code management tools and scripts. Please refer to the README file
  contained in the directory.


Additional hardware support
###########################

The framework supports a variety of hardware platforms such as different ARM
SoC families via supplemental repositories.

:Repositories maintained by Genode Labs:

  [https://github.com/orgs/genodelabs/repositories]


Additional community-maintained components
##########################################

The components found within the main source tree are complemented by a growing
library of additional software, which can be seamlessly integrated into Genode
system scenarios.

:Genode-world repository:

  [https://github.com/genodelabs/genode-world]


Community blog
##############

Genodians.org presents ideas, announcements, experience stories, and tutorials
around Genode, informally written by Genode users and developers.

:Genodians.org:

  [https://genodians.org]


Contact
#######

The best way to get in touch with Genode developers and users is the project's
mailing list. Please feel welcome to join in!

:Genode Mailing Lists:

  [https://genode.org/community/mailing-lists]


Commercial support
##################

The driving force behind the Genode OS Framework is the German company Genode
Labs. The company offers commercial licensing, trainings, support, and
contracted development work:

:Genode Labs website:

  [https://www.genode-labs.com]


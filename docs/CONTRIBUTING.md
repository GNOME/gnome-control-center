# Contributing

When contributing to the development of GNOME Settings, please first discuss the change you wish to
make via issue, email, or any other method with the maintainers before making a change.

Please note we have a Code of Conduct, please follow it in all your interactions with the project.

## Pull Request Process

1. Create a fork in GitLab and push your work to there
2. Open a Merge Request
  1. Always allow maintainer edits
  2. Mark the Merge Request as WIP if your work is not ready to be reviewed
3. Assign the correct maintainer to the Merge Request (see [`MAINTAINERS.md`][maintainers] to select
   the correct maintainer)
4. Format commit messages as follows:
   ```
   component: <summary>
   ‌
   A paragraph explaining the problem and its context.
‌
   Another one explaining how you solved that.
‌
   <link to the issue>
   ```
4. You may merge the pull request in once you have the sign-off of the maintainers, or if you
   do not have permission to do that, you may request the second reviewer to merge it for you.

## Code of Conduct

GNOME Settings is a project developed based on GNOME Code of Conduct and GitHub's community
guidelines. You can read it below:

### Summary

GNOME creates software for a better world. We achieve this by behaving well towards
each other.

Therefore this document suggests what we consider ideal behaviour, so you know what
to expect when getting involved in GNOME. This is who we are and what we want to be.
There is no official enforcement of these principles, and this should not be interpreted
like a legal document.

### Advice

 * **Be respectful and considerate**: Disagreement is no excuse for poor behaviour or personal
     attacks. Remember that a community where people feel uncomfortable is not a productive one.

 * **Be patient and generous**: If someone asks for help it is because they need it. Do politely
     suggest specific documentation or more appropriate venues where appropriate, but avoid
     aggressive or vague responses such as "RTFM".

 * **Assume people mean well**: Remember that decisions are often a difficult choice between
     competing priorities. If you disagree, please do so politely. If something seems outrageous,
     check that you did not misinterpret it. Ask for clarification, but do not assume the worst.

 * **Try to be concise**: Avoid repeating what has been said already. Making a conversation larger
     makes it difficult to follow, and people often feel personally attacked if they receive multiple
     messages telling them the same thing.


In the interest of fostering an open and welcoming environment, we as
contributors and maintainers pledge to making participation in our project and
our community a harassment-free experience for everyone, regardless of age, body
size, disability, ethnicity, gender identity and expression, level of experience,
nationality, personal appearance, race, religion, or sexual identity and
orientation.

### Communication Guidelines

It is of ultimate importance to maintain a community where everyone is free to express themselves,
review and comment on one another's ideas, both technical and otherwise. Such discussions, however,
are unlikely to foster fruitful dialog when ideas are silenced because community members are being
shouted down or are afraid to speak up.

If you are external to the GNOME community, always keep the assimetry of communication in mind: while
your interaction with community members (and in particular, maintainers and long-term contributors)
may only take a moment, these members usually have to interact with a significantly higher number of
people. Before writing a comment, opening a new issue, or engaging into any forum or IRC discussion,
please take a moment to appreciate that fact.

While communicating, it is expected that all involved members should be respectful and civil at all
times, and refrain from attacking others on the basis of who they are.

#### Communication Rules

The following behaviors will not be tolerated on any occasion:

 * **Threats of violence**: You may not threaten violence towards others or use the site to organize,
   promote, or incite acts of real-world violence or terrorism. Think carefully about the words you
   use, the images you post, and even the software you write, and how they may be interpreted by
   others. Even if you mean something as a joke, it might not be received that way. If you think
   that someone else might interpret the content you post as a threat, or as promoting violence or
   terrorism, stop. Don't post it. In extraordinary cases, we may report threats of violence to law
   enforcement if we think there may be a genuine risk of physical harm or a threat to public safety.

 * **Hate speech and discrimination**: While it is not forbidden to broach topics such as age, body
   size, disability, ethnicity, gender identity and expression, level of experience, nationality,
   personal appearance, race, religion, or sexual identity and orientation, we do not tolerate
   speech that attacks a person or group of people on the basis of who they are. Just realize that
   when approached in an aggressive or insulting manner, these (and other) sensitive topics can make
   others feel unwelcome, or perhaps even unsafe. While there's always the potential for
   misunderstandings, we expect our community members to remain respectful and civil when discussing
   sensitive topics.

 * **Bullying and harassment**: We do not tolerate bullying or harassment. This means any habitual
   badgering or intimidation targeted at a specific person or group of people. In general, if your
   actions are unwanted and you continue to engage in them, there's a good chance you are headed
   into bullying or harassment territory.

 * **Impersonation**: You may not seek to mislead others as to your identity by copying another
   person's avatar, posting content under their email address, using a similar username or otherwise
   posing as someone else. Impersonation is a form of harassment.

 * **Doxxing and invasion of privacy**: Don't post other people's personal information, such as
   phone numbers, private email addresses, physical addresses, credit card numbers, Social
   Security/National Identity numbers, or passwords. Depending on the context, such as in the case
   of intimidation or harassment, we may consider other information, such as photos or videos that
   were taken or distributed without the subject's consent, to be an invasion of privacy, especially
   when such material presents a safety risk to the subject.

 * **Obscene content**: Basically, don't post pornography, gore or any other depiction of violence.

#### General Advices

The following advices help increasing the efficiency of communication with community members:

 * Do not post "me too" comments. Use the GitLab reactions instead.
 * Avoid adding priority, time, or relevance hints if you are not involved with the development
   of the application. For example, `This is an urgent issue` or `This should be fixed now` or even
   `The majority of users need this feature`.
 * Do not use act or write using passive-aggressive communication.
 * When reporting technical problems with the application, such as misbehaviors or crashes, focus on
   sharing as many details as possible, and avoid adding non-technical information to it. An example
   of a **good** issue report:

   ```
   GNOME Settings crashes when opening the Wi-Fi panel with 3+ Wi-Fi adapters

   Steps to reproduce (assuming 3+ Wi-Fi adapters are present):

     1. Open GNOME Settings
     2. Select the Wi-Fi panel
     3. Observe the crash

   This does not happen with 2 or less adapters. Here is a backtrace of the
   crash: backtrace.txt
   ```

   In contrast, here is an example of a **bad** issue report:

   ```
   GNOME Settings crashed while I was trying to connect to the internet. How can such
   a thing happen and nobody notice? Did you not test it before releasing it?

   This should be fixed as quick as possible!
   ```
 * When asking for new features, try and add as many information to justify its relevance, why
   should it not be implemented as an auxiliary program, what problems would it solve, and offer
   suggestions about how you think it should be implemented. Example of a **good** feature request:

   ```
   GNOME Settings needs to expose IPv6 options

   As of now, the connection editor dialog does not allow editing various IPv6
   options. This is relevant because without some of these options, it is not
   possible to have a valid IPv6 configuration and, consequently, not have access
   to various websites and services.

   The list of missing configurations that are essential is:

    * <Feature A>
    * <Feature B>

   Optionally, the following configurations can also be added:

    * <Feature C>
    * <Feature D>

   Here is a quick sketch I have made showing how I think these options
   should be exposed as a user interface: sketch.png.
   ```

   Example of a **bad** feature request:

   ```
   Merge GNOME Tweaks in GNOME Settings

   The options in GNOME Tweaks are absolutely essential to the majority of us
   users. Why was it not merged already? This is an urgent issue and should
   have been addressed years ago. You should allocate all your resources on
   merging those two applications.
   ```

### What happens if someone breaks these rules or advices?

Actions that may be taken in response to an abusive comment include but are not limited to:

 * Content removal (when breaking any of the advices or rules)
 * Content blocking (when breaking any of the advices or rules)
 * Formal abuse report (when breaking any of the rules)

### Attribution

This Code of Conduct is adapted from the [Contributor Covenant][homepage], version 1.4,
available at [http://contributor-covenant.org/version/1/4][version]

[homepage]: http://contributor-covenant.org
[version]: http://contributor-covenant.org/version/1/4/
[maintainers]: https://gitlab.gnome.org/GNOME/gnome-control-center/blob/master/docs/MAINTAINERS.md

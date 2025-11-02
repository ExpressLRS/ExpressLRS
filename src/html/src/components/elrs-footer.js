import {html, LitElement} from 'lit'
import {customElement} from "lit/decorators.js";

@customElement('elrs-footer')
export class ElrsFooter extends LitElement {
    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <footer id="footer" class="elrs-header" style="align-content: center">
                <div class="mui--text-center mui-col-xs-4">
                    <a href="https://github.com/ExpressLRS/ExpressLRS">
                        <svg width="24" xmlns="http://www.w3.org/2000/svg" aria-label="GitHub" role="img"
                             viewBox="0 0 512 512">
                            <rect width="512" height="512" rx="15%" fill="#181717"/>
                            <path fill="#fff"
                                  d="M335 499c14 0 12 17 12 17H165s-2-17 12-17c13 0 16-6 16-12l-1-44c-71 16-86-34-86-34-12-30-28-37-28-37-24-16 1-16 1-16 26 2 40 26 40 26 22 39 59 28 74 22 2-17 9-28 16-35-57-6-116-28-116-126 0-28 10-51 26-69-3-6-11-32 3-67 0 0 21-7 70 26 42-12 86-12 128 0 49-33 70-26 70-26 14 35 6 61 3 67 16 18 26 41 26 69 0 98-60 120-117 126 10 8 18 24 18 48l-1 70c0 6 3 12 16 12z"/>
                        </svg>
                        <br>
                        GitHub
                    </a>
                </div>
                <div class="mui--text-center mui-col-xs-4">
                    <a href="https://discord.gg/dS6ReFY">
                        <svg width="24" fill="#5865f2" aria-label="Discord" role="img" version="1.1"
                             viewBox="0 0 512 512" xmlns="http://www.w3.org/2000/svg">
                            <rect width="512" height="512" rx="15%" fill="#fff"/>
                            <path d="m386 137c-24-11-49.5-19-76.3-23.7c-.5 0-1 0-1.2.6c-3.3 5.9-7 13.5-9.5 19.5c-29-4.3-57.5-4.3-85.7 0c-2.6-6.2-6.3-13.7-10-19.5c-.3-.4-.7-.7-1.2-.6c-23 4.6-52.4 13-76 23.7c-.2 0-.4.2-.5.4c-49 73-62 143-55 213c0 .3.2.7.5 1c32 23.6 63 38 93.6 47.3c.5 0 1 0 1.3-.4c7.2-9.8 13.6-20.2 19.2-31.2c.3-.6 0-1.4-.7-1.6c-10-4-20-8.6-29.3-14c-.7-.4-.8-1.5 0-2c2-1.5 4-3 5.8-4.5c.3-.3.8-.3 1.2-.2c61.4 28 128 28 188 0c.4-.2.9-.1 1.2.1c1.9 1.6 3.8 3.1 5.8 4.6c.7.5.6 1.6 0 2c-9.3 5.5-19 10-29.3 14c-.7.3-1 1-.6 1.7c5.6 11 12.1 21.3 19 31c.3.4.8.6 1.3.4c30.6-9.5 61.7-23.8 93.8-47.3c.3-.2.5-.5.5-1c7.8-80.9-13.1-151-55.4-213c0-.2-.3-.4-.5-.4Zm-192 171c-19 0-34-17-34-38c0-21 15-38 34-38c19 0 34 17 34 38c0 21-15 38-34 38zm125 0c-19 0-34-17-34-38c0-21 15-38 34-38c19 0 34 17 34 38c0 21-15 38-34 38z"
                                  fill="#5865f2"/>
                        </svg>
                        <br>
                        Discord
                    </a>
                </div>
                <div class="mui--text-center mui-col-xs-4">
                    <a href="https://www.facebook.com/groups/636441730280366">
                        <svg width="24" xmlns="http://www.w3.org/2000/svg" aria-label="Facebook" role="img"
                             viewBox="0 0 512 512">
                            <rect width="512" height="512" rx="15%" fill="#1877f2"/>
                            <path d="M355.6 330l11.4-74h-71v-48c0-20.2 9.9-40 41.7-40H370v-63s-29.3-5-57.3-5c-58.5 0-96.7 35.4-96.7 99.6V256h-65v74h65v182h80V330h59.6z"
                                  fill="#fff"/>
                        </svg>
                        <br>
                        Facebook
                    </a>
                </div>
            </footer>
        `
    }
}
